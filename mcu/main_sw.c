/**
 * Image filtering and segmentation on MSP430 and FPGA SPARTAN3
 * by Jakub Vojvoda, vojvoda@swdeveloper.sk
 * 2014
 *
 **/

#include <fitkitlib.h>
#include "../cpu/common.h"

//#define PROFILE

const int img_size[4]   = {8,16,32,64};
const int prime_num[16] = {919, 929, 937, 941, 947, 953, 967, 971, \
                           977, 983, 991, 997, 1009, 1013, 1019, 1021};

/* 1 tik casovace = 30,518 us */
#define TIMER_TICK 30.518

typedef unsigned char t_pixel_sw;

/* globalni promenne */
unsigned long global_time=0;


/*******************************************************************************
 * Aktualizace globalniho casu
*******************************************************************************/
void global_time_update() {

   static unsigned long act_time, prev_time = 0;

   CCTL0 &= ~CCIE;  // disable interrupt               

   act_time = TAR;

   if (act_time >= prev_time)
      global_time += (act_time - prev_time); // v ramci stejneho cyklu
   else
      global_time += (act_time + (65536 - prev_time)); // preteceni

   prev_time = act_time;

   CCTL0 = CCIE;  // enable interrupt               
}

/*******************************************************************************
 * Aktualizace globalniho casu
*******************************************************************************/
unsigned long get_time() {

   global_time_update();

   return global_time;
}

/*******************************************************************************
 * Obsluha preruseni casovace timer A0
*******************************************************************************/
interrupt (TIMERA0_VECTOR) Timer_A (void) {

   global_time_update();

   CCR0 += 0x80;
}

/*******************************************************************************
 * Vypis uzivatelske napovedy (funkce se vola pri vykonavani prikazu "help")
*******************************************************************************/
void print_user_help(void) {
}

/*******************************************************************************
 * Dekodovani uzivatelskych prikazu a jejich vykonavani
*******************************************************************************/
unsigned char decode_user_cmd(char *cmd_ucase, char *cmd) {
   return (CMD_UNKNOWN);
}

/*******************************************************************************
 * Inicializace periferii/komponent po naprogramovani FPGA
*******************************************************************************/
void fpga_initialized() {
}

/***************************************************************************
 Procedura update_base_pos zajistuje aktualizaci bazove pozice, ktera posouva
 objekt po obrazovce
***************************************************************************/
void update_base_pos(unsigned int *base_r, unsigned int *base_c) {

   static int inc_r = 1, inc_c = 1;

   if (*base_r == BASE_LST_ROW) {
         inc_r = -1;
   }
   else if (*base_r == BASE_FST_ROW) {
         inc_r = 1;
   }

   if (*base_c == BASE_LST_COL) {
         inc_c = -1;
   }
   else if (*base_c == BASE_FST_COL) {
         inc_c = 1;
   }

   *base_r = (*base_r) + inc_r;
   *base_c = (*base_c) + inc_c;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef CPU_H
	/***************************************************************************
	 Funkce otsu() vypocte hodnotu prahu na zaklade histogramu pixelu snimku.

	 Vstupy:
		hist    ukazatel na histogram
		n       pocet polozek histogramu
	 Vystupy:
		hodnota vypocteneho prahu
	***************************************************************************/
	int otsu(unsigned long int *hist, int n) {

	   int   total = 0;
	   float sum = 0;
	   float sumB = 0, varMax = 0, varBetween;
	   int   wB = 0,  wF = 0, threshold = 0;
	   float mB, mF;  
	   int   t;

	   for (t=0 ; t<n ; t++) {
		  sum += t * hist[t];
		  total += hist[t];
	   }

	   for (t=0 ; t<n; t++) {

		  wB += hist[t];             /* Weight Background */
		  if (wB == 0) continue;

		  wF = total - wB;           /* Weight Foreground */
		  if (wF == 0) break;

		  sumB += (float) (t * hist[t]);

		  mB = sumB / wB;            /* Mean Background */
		  mF = (sum - sumB) / wF;    /* Mean Foreground */

		  /* Calculate Between Class Variance */
		  varBetween = (float)wB * (float)wF * (mB - mF) * (mB - mF);

		  /* Check if new maximum found */
		  if (varBetween > varMax) {
			 varMax = varBetween;
			 threshold = t+1;
		  }
	   }

	   return threshold;
	}

	/***************************************************************************
	 Funkce median() vraci hodnotu medianu ze zadaneho okenka hodnot 3x3 pixelu.

	 Vstupy:
	   window   ukazatel na hodnoty okenka 3x3 pixelu
	 Vystupu:
	   hodnota medianu
	***************************************************************************/
	t_pixel_sw median(t_pixel_sw *window){

	   int         i, j, max;
	   t_pixel_sw  R[9];

	   for(i=0;i<9;i++)
		  R[i] = window[i];

	   for(j=0;j<4;j++){
		  max=j;
		  for(i=j+1;i<9;i++)
			 if(R[i]>R[max]) max=i;
		  R[max]=R[j];
	   }

	   max=4;
	   for(i=5;i<9;i++)
		  if(R[i]>R[max]) max=i;

	   return R[max];
	}

	/***************************************************************************
	 Procedura buffer() uklada vstupni hodnoty pixelu do dvou-radkovou bufferu
	 a vraci hodnoty odpovidajici aktualne zpracovavanemu sloupci 3 pixelu
	 (posledni sloupec okenka 3x3).

	 Vstupy:
	   din   hodnota vstupniho pixelu
	   c     cislo sloupce prave zpracovavaneho pixelu
	 Vystupy:
	   col_window  aktualne zpracovavany sloupec tri pixelu
	***************************************************************************/
	void buffer(t_pixel_sw din, int c, t_pixel_sw *col_window){

	   static t_pixel_sw buf[2][FRAME_COLS];
	   static int        sel=1;
	   t_pixel_sw        t0, t1;

	   if (c==0) sel = !sel;
	   t1 = buf[1][c];
	   t0 = buf[0][c];
	   buf[sel][c] = din;

	   col_window[0] = (sel==1) ? t1:t0;
	   col_window[1] = (sel==1) ? t0:t1;
	   col_window[2] = din;
	}

	/***************************************************************************
	 Procedura clip_window() provadi clipping tj. doplnuje krajni hodnoty okenka
	 3x3 na okrajich snimku, kde nejsou pixely k dispozici.

	 Vstupy:
	   r     aktualni cislo radku
	   c     aktualni cislo sloupce
	   iw    aktualni hodnoty posuvneho okenka 3x3
	 Vystupy:
	   ow    upravene hodnoty posuvneho okenka 3x3 po osetreni krajnich hodnot
	***************************************************************************/
	void clip_window(int r, int c, t_pixel_sw *iw, t_pixel_sw *ow) {
	   
	   int first_row, last_row, first_col, last_col;
	   int test1, test2, test3, test4;

	   first_row = (r == 0);
	   first_col = (c == 0);
	   last_row  = (r == FRAME_ROWS-1);
	   last_col  = (c == FRAME_COLS-1);

	   ow[4] = iw[4];
	   ow[1] = first_col ? iw[4] : iw[1];
	   ow[5] = last_row  ? iw[4] : iw[5];
	   ow[7] = last_col  ? iw[4] : iw[7];
	   ow[3] = first_row ? iw[4] : iw[3];

	   test1 = first_row | (first_col << 1);
	   switch(test1) {
		  case 3:  ow[0] = iw[4]; break; /* first_row, first_col */
		  case 1:  ow[0] = iw[1]; break; /* first_row, not first_col */
		  case 2:  ow[0] = iw[3]; break; /* not first_row, first_col */
		  default: ow[0] = iw[0]; break; /* not first_row, not first_col */
	   }

	   test2 = first_row | (last_col << 1);
	   switch(test2) {
		  case 3:  ow[6] = iw[4]; break; /* first_row, last_col */
		  case 1:  ow[6] = iw[7]; break; /* first_row, not last_col */
		  case 2:  ow[6] = iw[3]; break; /* not first_row, last_col */
		  default: ow[6] = iw[6]; break; /* not first_row, not last_col */
	   }

	   test3 = last_row | (first_col << 1);
	   switch(test3) {
		  case 3:  ow[2] = iw[4]; break; /* last_row, first_col */
		  case 1:  ow[2] = iw[1]; break; /* last_row, not first_col */
		  case 2:  ow[2] = iw[5]; break; /* not last_row, first_col */
		  default: ow[2] = iw[2]; break; /* not last_row, not first_col */
	   }

	   test4 = last_row | (last_col << 1);
	   switch(test4) {
		  case 3:  ow[8] = iw[4]; break; /* last_row, last_col */
		  case 1:  ow[8] = iw[7]; break; /* last_row, not last_col */
		  case 2:  ow[8] = iw[5]; break; /* not last_row, last_col */
		  default: ow[8] = iw[8]; break; /* not last_row, not last_col */
	   }
	}

	/***************************************************************************
	 Procedura shift_window() provadi posun okenka 3x3 o jednu pozici do prava.

	 Vstupy:
	   window      puvodni hodnoty posuvneho okenka 3x3
	   col_window  nove nasouvany sloupec  
	 Vystupy:
	   window      hodnoty vstupniho pole jsou aktualizovany
	***************************************************************************/
	void shift_window(t_pixel_sw *window, t_pixel_sw *col_window) {

	   window[2] = window[5];
	   window[1] = window[4];
	   window[0] = window[3];

	   window[5] = window[8];
	   window[4] = window[7];
	   window[3] = window[6];

	   window[8] = col_window[2];
	   window[7] = col_window[1];
	   window[6] = col_window[0];
	}

	/***************************************************************************
	 Funkce system_input() zajistuje zpracovani a bufferovani vstupnich pixelu.
	 Vstupni pixel ulozi do radkoveho bufferu a provede posun a clipping posuvneho
	 okenka. Funkce rozlisuje mezi vstupnim pixelem a skutecne filtrovanym
	 pixelem. Filtrovany pixel je oproti vstupnimu posunut o jeden radek a jeden
	 pixel.

	 Vstupy:
	   din            vstupni pixel
	 Vystupy:
	   cliped_window  posuvne okenko 3x3 po osetreni okrajovych bodu
	   last_pixel     infomace o tom, zda se jedna o posledni pixel snimku
	   navratova hodnota ukazu, zda je okenko platne ci nikoliv. Okenko nemusi
	   byt platne napr. na zacatku zpracovani, kdy jeste neni v bufferu nasunut 
	   dostatek novych pixelu
	***************************************************************************/
	int system_input(t_pixel_sw din, t_pixel_sw *cliped_window, int *last_pixel){

	   static int        c = 0, r = 0;
	   static int        c_filter = 0, r_filter = 0;
	   static int        output_vld = 0;
	   static t_pixel_sw window[9];
	   t_pixel_sw        col_window[3];

	   /* ulozeni pixelu do bufferu, posun okenka a clipping */
	   buffer(din, c, col_window);
	   shift_window(window, col_window);
	   clip_window(r_filter, c_filter, window, cliped_window);

	   /* od druheho radku a druheho sloupce je vystup platny */
	   if ((r == 1) && (c == 1))
		  output_vld = 1;

	   /* oznaceni posledniho filtrovaneho pixelu snimku */
	   *last_pixel = ((r_filter == FRAME_ROWS-1) && (c_filter == FRAME_COLS-1));

	   /* aktualizace souradnic filtrovaneho pixelu */
	   if ((c_filter == FRAME_COLS-1) && (output_vld == 1)) {
		  r_filter = (r_filter == FRAME_ROWS-1) ? 0 : r_filter+1;
	   }
	   c_filter = c;

	   /* aktualizace souradnic vstupniho pixelu */
	   if (c == FRAME_COLS-1) {
		  c = 0;
		  r = (r == FRAME_ROWS-1) ? 0 : r+1;
	   }
	   else  
		  c++;

	   return output_vld;
	}

	/***************************************************************************
	 Procedura histogram_clean() zajistuje vymazani histogramu, ktery je potreba
	 zejmena na konci zpracovani kazdeho snimku.

	 Vstupy:
	   hist     adresa histogramu
	***************************************************************************/
	void histogram_clean(unsigned long int *hist) {

	   int i;

	   for(i=0; i<PIXELS; i++)
		  hist[i] = 0;
	}

	/***************************************************************************
	 Funkce thresholding() provadi prahovani vstupniho pixelu vuci zadanemu
	 prahu.

	 Vstupy:
	   pixel       vstupni pixel
	   threshold   hodnota prahu
	 Vystupy:
	   navratova hodnota reprezentuje vystupni pixel po provedeni prahovani
	***************************************************************************/
	t_pixel_sw thresholding(t_pixel_sw pixel, int threshold) {

	   if (pixel >= threshold)
		  return MAX_PIXEL;
	   else
		  return MIN_PIXEL;
	}

	/***************************************************************************
	 Pomocna procedura pro tisk vysledku

	 Vstupy:
	   frame       cislo sminku
	   threshold   vypocteny prah
	   hist        adresa histogramu
	   n           pocet polozek histogramu
	***************************************************************************/
	void print_results(int frame, int threshold, unsigned long int *hist, int n) {

	   int i;

	   term_send_str("Frame: "); 
	   term_send_num(frame); 
	   term_send_crlf();
	   
	   term_send_str("Histogram: "); 
	   term_send_num(hist[0]);    
	   
	   for(i=1; i<n; i++) {
		  term_send_str(", ");
		  term_send_num(hist[i]);
	   }
	   
	   term_send_crlf();

	   term_send_str("Threshold: "); 
	   term_send_num(threshold); 
	   term_send_crlf();
	}

	/***************************************************************************
	 Procedura pixel_processing() zajistuje kompletni zpracovani vstupniho
	 pixelu. Nejprve je vstupni pixel nacten a ulozen do bufferu. Na zaklade
	 posuvneho okenka 3x3 se provede filtrace medianovych filtrem. Nasledne se
	 aktualizuji hodnoty histogramu snimku a provede se prahovani. Na konci
	 snimku se na zaklade histogramu vypocte nova hodnota prahu a hodnoty
	 puvodniho histogramu se vynuluji.

	 Vstupy:
	   data_in        vstupni pixel

	 Vystupy:
	   data_out       vystupni pixel
	   data_out_vld   informace o tom, zda je vystupni pixel platny
	***************************************************************************/
	void pixel_processing(t_pixel_sw data_in, t_pixel_sw *data_out, int *data_out_vld) {

	   static unsigned long int  histogram[PIXELS] = {0, 0, 0, 0, 0, 0, 0, 0};
	   static int  threshold = 4, new_threshold = 4;
	   static int  frame=100;
	   t_pixel_sw  pix_filtered, window[9];
	   int         last_pixel;


	   *data_out_vld = system_input(data_in, window, &last_pixel);
	   if(*data_out_vld) {
		  /* Filtrace medianem, aktualizace histogramu, prahovani */
		  pix_filtered = median(window);
		  if((frame % 10) == 0)
			 histogram[pix_filtered]++;
		  *data_out = thresholding(pix_filtered, threshold);

		  /* Test na posledni pixel predchoziho snimku */
		  if (last_pixel) {
			 if((frame % 10) == 0) {

				new_threshold = otsu(histogram, PIXELS);
	#ifndef PROFILE
				if ((frame % 100) == 0)
				   print_results(frame, new_threshold, histogram, PIXELS);
	#endif
				histogram_clean(histogram);
			 /* V realne aplikaci bude vypocet noveho prahu trvat urcitou dobu, a
				proto nebude mozne jej aplikovat hned na nasledujici snimek. Jinymi
				slovy, pokud je histogram vypocten pro snimek s indexem "i", potom
				vypocet prahu bude realizovan v ramci snimku "i+1" a novy prah bude
				aplikovan az od prvniho pixelu snimku "i+2". */
			 } else if ((frame % 10) == 1)
				threshold = new_threshold;
			 frame += 100;
		  }
	   }
	}
#endif
//////////////////////////////////////////////////////////////////////////////////////////////////////////////

/***************************************************************************
 Funkce gen_pixel() zajistuje generovani vstupnich pixelu.

 Vstupy
   frame_inc   pocet sminku, o ktere se ma generator pixelu posunout vpred.
               Pouze pro ucely simulace algoritmu na mikrokontroleru, ktery 
               ma nizky vypocetni vykon.

 Vystupy:
   navratova hodnota reprezentuje nove vygenerovany pixel
***************************************************************************/
t_pixel_sw gen_pixel(unsigned int frame_inc) {

   static unsigned int        r=0, c=0, base_r=100, base_c=100;
   static unsigned int        noise_cnt = 0, frame_cnt = 0;
   t_pixel_sw                 pixel_int, noise_pix, diff, diff_pix;
   unsigned int               diff_r, diff_c, i; 
   unsigned long              noise_inc;

   /* posun generatoru o nekolik smimku vpred */
   if(frame_inc > 0) {
      for (i = 0; i < frame_inc; i++)
         update_base_pos(&base_r, &base_c);
      noise_inc = (long)FRAME_ROWS*(long)FRAME_COLS*(long)frame_inc;
      noise_cnt =  (noise_cnt + noise_inc) % NOISE_INTERVAL;
      frame_cnt += frame_inc;
      return 0;
   }

   /* vypocet rozdilu aktualni pozice vuci bazove pozici */
   diff_r = r - base_r;
   diff_c = c - base_c;
   diff = (diff_r > diff_c) ? (diff_r >> NUM1) : (diff_c >> NUM1);
   diff = ~diff & MAX_PIXEL;
   diff_pix = diff + ((frame_cnt >> 3) & MAX_PIXEL);
   if (diff_pix > MAX_PIXEL)
      diff_pix = MAX_PIXEL;

   /* vykresli pixel pokud je v blizkosti bazovych souradnic */
   if ((diff_c < IMG_SIZE) && (diff_r < IMG_SIZE))
      pixel_int = diff_pix;
   else
      pixel_int = MIN_PIXEL;

   noise_pix = (noise_cnt == NOISE_INTERVAL-1) ? MAX_PIXEL : MIN_PIXEL;

   pixel_int |= noise_pix;

   /* aktualizace aktualnich souradnic */
   if (c == FRAME_COLS-1) {
      c = 0;
      if (r == FRAME_ROWS-1) {
         r = 0;
         /* na konci smiku - aktualizace bazovych souradnic */
         update_base_pos(&base_r, &base_c);
         frame_cnt++;
      }
      else
         r++;
   }
   else
      c++;

   /* citac sumu */
   noise_cnt =  (noise_cnt == NOISE_INTERVAL-1) ? 0 : noise_cnt+1;

   return pixel_int;
}

/*******************************************************************************
 * Hlavni funkce
*******************************************************************************/
int main(void)
{
   short counter = 0;
   unsigned long start_time, end_time;

   initialize_hardware();
   set_led_d6(1);  //rozsvitit LED D6
   set_led_d5(1);  //rozsvitit LED D5

   /* nastaveni casovace na periodu 10 ms */
   CCTL0 = CCIE;  // enable interrupt               
   CCR0 = 0x80;
   TACTL = TASSEL_1 + MC_2;

   /**************************************************************************/
   /*                          Hlavni program                                */
   /**************************************************************************/
   
   int         r, c, f;
   t_pixel_sw  pix_input, pix_output;
   int         pix_output_vld;

   for (f = 0; f < FRAMES; f += 100) {

	   gen_pixel(99);

	   for (r = 0; r < FRAME_ROWS; r++) {
		   for (c = 0; c < FRAME_COLS; c++) {
            
			   pix_input = gen_pixel(0);			   
			   pixel_processing(pix_input, &pix_output, &pix_output_vld);
         }
      }
   }

   CCTL0 &= ~CCIE;  // disable interrupt
   /**************************************************************************/

   set_led_d5(0);  //zhasnout LED D5

   while (1) {
      delay_ms(1);  //zpozdeni 1ms

      counter++;
      if (counter == 500) {
         flip_led_d6(); //invertovat LED
         counter = 0;
      }

      terminal_idle();  // obsluha terminalu
   }
}

