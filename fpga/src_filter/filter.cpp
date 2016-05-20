/**
 * Image filtering and segmentation on MSP430 and FPGA SPARTAN3
 * by Jakub Vojvoda, vojvoda@swdeveloper.sk
 * 2014
 *
 **/

#include <ac_int.h>
#include "filter.h"
#include "addr_space.h"

#define WRITE_NOT_VALID      0

#define WRITE_HIST_0         1
#define WRITE_HIST_1         2
#define WRITE_HIST_2         3
#define WRITE_HIST_3         4
#define WRITE_HIST_4         5
#define WRITE_HIST_5         6
#define WRITE_HIST_6         7
#define WRITE_HIST_7         8
#define WRITE_FRAME          9
#define WRITE_DONE          10

#ifndef CPU_H
    /***************************************************************************
     Funkce median() vraci hodnotu medianu ze zadaneho okenka hodnot 3x3 pixelu.
     Jedna se o paralelni verzi algoritmu vhodnou pro hardware. 

     Vstupy:
       window   ukazatel na hodnoty okenka 3x3 pixelu
     Vystupu:
       pix_out  hodnota medianu
    ***************************************************************************/
    t_pixel median(t_pixel *window){

       int i, j;
       t_pixel max[10], max2[10];

       Linit: for(i=0;i<9;i++)
          max[i] = window[i];
       max[9] = MIN_PIXEL;
       
       L2: for(j=0;j<5;j++) {

           L1a: for(i=0;i<5;i++) {
              if(max[i*2]<max[(i*2)+1]) {
                  max2[i*2]     = max[i*2]; 
                  max2[(i*2)+1] = max[(i*2)+1];
              }
              else {
                  max2[i*2]     = max[(i*2)+1]; 
                  max2[(i*2)+1] = max[i*2];
              }
           }

           L1b: for(i=0;i<4;i++) {
              if(max2[(i*2)+1]<max2[(i*2)+2]) {
                  max[(i*2)+1] = max2[(i*2)+1]; 
                  max[(i*2)+2] = max2[(i*2)+2];
              }
              else {
                  max[(i*2)+1] = max2[(i*2)+2]; 
                  max[(i*2)+2] = max2[(i*2)+1];
              }
           }     
           max[0] = max2[0];
           max[9] = max2[9];
       }

       return max[5];
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
	void buffer(t_pixel din, int c, t_pixel *col_window){

	   static t_pixel buf[2][FRAME_COLS];
	   static int        sel=1;
	   t_pixel           t0, t1;

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
	void clip_window(int r, int c, t_pixel *iw, t_pixel *ow) {
	   
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
	void shift_window(t_pixel *window, t_pixel *col_window) {

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
	int system_input(t_pixel din, t_pixel *cliped_window, int *last_pixel){

	   static int        c = 0, r = 0;
	   static int        c_filter = 0, r_filter = 0;
	   static int        output_vld = 0;
	   static t_pixel    window[9];
	   t_pixel           col_window[3];

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

#endif

/***************************************************************************
 Procedura filter() zajistuje zpracovani vstupniho pixelu. Tato jednoducha
 verze provadi pouze kopirovani vstupniho pixelu na vystup.

 Vstupy:
   in_data        vstupni pixel
   in_data_vld    informace o tom, zda je vstupni pixel platny
   mcu_data       pamet pro vymenu dat s MCU

 Vystupy:
   out_data       vystupni pixel
   mcu_data       pamet pro vymenu dat s MCU
***************************************************************************/
#pragma hls_design top

void filter(t_pixel &in_data, bool &in_data_vld, t_pixel &out_data, 
         t_mcu_data mcu_data[MCU_SIZE]){

    static ac_int<17, false>  histogram[PIXELS] = {0, 0, 0, 0, 0, 0, 0, 0};
    static ac_int< 4, false>  threshold = 4, new_threshold = 4;
	   
    t_pixel  pix_filtered, window[9];
    int last_pixel;
    bool out_data_vld = false;
    
    static int frame = 1;	  
    static ac_int< 4, false> state = WRITE_NOT_VALID;
    
    if (in_data_vld) {
        out_data_vld = system_input(in_data, window, &last_pixel);
    
        if (out_data_vld) {
            
            pix_filtered = median(window);
            
            if((frame % 10) == 0)
		        histogram[pix_filtered]++;
            
            if (pix_filtered >= threshold)
                out_data = MAX_PIXEL;
            else
                out_data = MIN_PIXEL;
            
            if (last_pixel) {
            
                if ((frame % 10) == 1)
                    threshold = mcu_data[FPGA_THRESH];         
                
                state = WRITE_HIST_0;
                mcu_data[FPGA_FRAME] = frame;
                
                frame++;
            } 
        }
    }
    
    switch(state){
        
        case WRITE_NOT_VALID:
            break;
        
        case WRITE_HIST_0:
            mcu_data[FPGA_HIST_0] = histogram[0];
            histogram[0] = 0;
            state = WRITE_HIST_1;
            break;
        
        case WRITE_HIST_1:
            mcu_data[FPGA_HIST_1] = histogram[1];
            histogram[1] = 0;
            state = WRITE_HIST_2;
            break;
            
        case WRITE_HIST_2:
            mcu_data[FPGA_HIST_2] = histogram[2];
            histogram[2] = 0;
            state = WRITE_HIST_3;
            break;
            
        case WRITE_HIST_3:
            mcu_data[FPGA_HIST_3] = histogram[3];
            histogram[3] = 0;
            state = WRITE_HIST_4;
            break;
            
        case WRITE_HIST_4:
            mcu_data[FPGA_HIST_4] = histogram[4];
            histogram[4] = 0;
            state = WRITE_HIST_5;
            break;
            
        case WRITE_HIST_5:
            mcu_data[FPGA_HIST_5] = histogram[5];
            histogram[5] = 0;
            state = WRITE_HIST_6;
            break;
            
        case WRITE_HIST_6:
            mcu_data[FPGA_HIST_6] = histogram[6];
            histogram[6] = 0;
            state = WRITE_HIST_7;
            break;

        case WRITE_HIST_7:
            mcu_data[FPGA_HIST_7] = histogram[7];
            histogram[7] = 0;
            state = WRITE_DONE;
            break;
        
        case WRITE_DONE:
            mcu_data[FPGA_VALID] = 1;
            state = WRITE_NOT_VALID;
            break;
        
        default: break;
    }
}

