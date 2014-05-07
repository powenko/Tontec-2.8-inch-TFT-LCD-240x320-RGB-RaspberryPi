#include "bcm2835.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/timeb.h>

#define RGB565(r,g,b)((r >> 3) << 11 | (g >> 2) << 5 | ( b >> 3))
#define BCM2708SPI
#define ROTATE90

#define RGB565_MASK_RED        0xF800
#define RGB565_MASK_GREEN    0x07E0
#define RGB565_MASK_BLUE       0x001F

#ifdef    ROTATE90
	#define XS    0x0006
	#define XE    0x0008
	#define YS    0x0002
	#define YE    0x0004
	#define MAX_X    319
	#define MAX_Y    239
#else
	#define XS    0x02
	#define XE    0x04
	#define YS    0x06
	#define YE    0x08
	#define MAX_X    239
	#define MAX_Y    319
#endif

#define	SPICS	RPI_GPIO_P1_24	//GPIO08
#define	SPIRS	RPI_GPIO_P1_22	//GPIO25
#define	SPIRST	RPI_GPIO_P1_16	//GPIO23
#define	SPISCL	RPI_GPIO_P1_23	//GPIO11
#define	SPISCI	RPI_GPIO_P1_19	//GPIO10
#define	LCDPWM	RPI_GPIO_P1_12	//GPIO18

#define LCD_CS_CLR	bcm2835_gpio_clr(SPICS)
#define LCD_RS_CLR	bcm2835_gpio_clr(SPIRS)
#define LCD_RST_CLR	bcm2835_gpio_clr(SPIRST)
#define LCD_SCL_CLR	bcm2835_gpio_clr(SPISCL)
#define LCD_SCI_CLR	bcm2835_gpio_clr(SPISCI)
#define LCD_PWM_CLR	bcm2835_gpio_clr(LCDPWM)

#define LCD_CS_SET	bcm2835_gpio_set(SPICS)
#define LCD_RS_SET	bcm2835_gpio_set(SPIRS)
#define LCD_RST_SET	bcm2835_gpio_set(SPIRST)
#define LCD_SCL_SET	bcm2835_gpio_set(SPISCL)
#define LCD_SCI_SET	bcm2835_gpio_set(SPISCI)
#define LCD_PWM_SET	bcm2835_gpio_set(LCDPWM)


short color[]={0xf800,0x07e0,0x001f,0xffe0,0x0000,0xffff,0x07ff,0xf81f};

/* Image part */
char *value=NULL;
int hsize=0, vsize=0;

void resize (int new_hsize, int new_vsize)
{
    char *new_value = (char *)malloc(new_hsize*new_vsize*3);
    int i;
    
    for (i = 0; i < new_hsize*new_vsize*3; i++)
        new_value[i] = 0;
    
    hsize = new_hsize;
    vsize = new_vsize;
    
    if (value != NULL)
        free(value);
    value = new_value;
}


void PGMSkipComments (FILE* infile, unsigned char* ch)
{
    while ((*ch == '#')) {
        while (*ch != '\n') { *ch = fgetc(infile); }
        while (*ch <  ' ' ) { *ch = fgetc(infile); }
    }
}

unsigned int PGMGetVal (FILE* infile)
{
    unsigned int tmp;
    unsigned char ch;
    do { ch = fgetc(infile); } while ((ch <= ' ') && (ch != '#'));
    PGMSkipComments(infile, &ch);
    ungetc(ch, infile);
    if (fscanf(infile,"%u",&tmp) != 1) {
        printf ("%s\n","Error parsing file!");
        exit(1);
    }
    return(tmp);
}

void PGMLoadData (FILE *infile, const char *filename)
{
    unsigned char *buffer;
    int i;
    long fp;
    buffer = (unsigned char *)malloc(hsize * vsize*3);
    
    fp = -1*hsize*vsize*3;
    fseek(infile, fp, SEEK_END);
    
    if (fread (buffer, hsize*vsize*3, sizeof(unsigned char), infile) != 1)
        printf ("Read < %d chars when loading file %s\n", hsize*vsize*3, filename);
    
    for (i = 0; i < hsize*vsize*3; i++)
        value[i] = buffer[i];
    
    free(buffer);
}

void loadPGM (const char *filename)
{
    FILE* infile;
    unsigned char ch = ' ';
    int xsize=0, ysize=0, maxg=0;
    
    infile = fopen (filename, "rb");
    if (infile == NULL)
        printf ("Unable to open file %s\n", filename);
    
    // Look for type indicator
    while ((ch != 'P') && (ch != '#')) { ch = fgetc(infile); }
    PGMSkipComments(infile, &ch);
    char ftype = fgetc(infile); // get type, 5 or 6
    
    // Look for x size, y size, max grey level
    xsize = (int)PGMGetVal(infile);
    ysize = (int)PGMGetVal(infile);
    maxg = (int)PGMGetVal(infile);
    
    // Do some consistency checks
    
    if ( (hsize <= 0) && (vsize <= 0) ) {
        resize (xsize, ysize);
        if (value == NULL)
            printf ("Can't allocate memory for image of size %d by %d\n",
                    hsize, vsize);
    } else {
        if ((xsize != hsize) || (ysize != vsize)) {
            printf ("File dimensions conflict with image settings\n");
        }
    }
    
    if (ftype == '7') {
        printf("File %s is of type PGM, is %d x %d with max gray level %d\n",
               filename, hsize, vsize, maxg);
        PGMLoadData(infile, filename);
    }
    if (ftype == '6') {
        printf("File %s is of type PPM, is %d x %d with max gray level %d\n",
               filename, hsize, vsize, maxg);
        PGMLoadData(infile, filename);
    }
    
    fclose(infile);
}


void LCD_WR_REG(int index)
{
	char i;
	LCD_CS_CLR;
	LCD_RS_CLR;
#ifdef BCM2708SPI
	bcm2835_spi_transfer(index>>8);
	bcm2835_spi_transfer(index);
#else
    
	for(i=0;i<16;i++)
	{
		LCD_SCL_CLR;
		if(index&0x8000)
		{
			LCD_SCI_SET;
		}
		else
		{
			LCD_SCI_CLR;
		}
		index=index<<1;
		LCD_SCL_SET;
	}
#endif
	LCD_CS_SET;
    
}

int compare(const void *a, const void *b)
{
    return *(int *)a - *(int *)b;
}

void LCD_WR_CMD(int index,int val)
{
	char i;
	LCD_CS_CLR;
	LCD_RS_CLR;
#ifdef BCM2708SPI
	bcm2835_spi_transfer(index>>8);
	bcm2835_spi_transfer(index);
#else
	for(i=0;i<16;i++)
	{
		LCD_SCL_CLR;
		if(index&0x8000)
		{
			LCD_SCI_SET;
		}
		else
		{
			LCD_SCI_CLR;
		}
		index=index<<1;
		LCD_SCL_SET;
	}
#endif
	LCD_RS_SET;
#ifdef BCM2708SPI
	bcm2835_spi_transfer(val>>8);
	bcm2835_spi_transfer(val);
#else
	for(i=0;i<16;i++)
	{
		LCD_SCL_CLR;
		if(val&0x8000)
		{
			LCD_SCI_SET;
		}
		else
		{
			LCD_SCI_CLR;
		}
		val=val<<1;
		LCD_SCL_SET;
	}
#endif
	LCD_CS_SET;
}

void inline LCD_WR_Data(int val)
{
	char i;
#ifdef BCM2708SPI
	bcm2835_spi_transfer(val>>8);
	bcm2835_spi_transfer(val);
#else
	for(i=0;i<16;i++)
	{
		LCD_SCL_CLR;
		if(val&0x8000)
		{
			LCD_SCI_SET;
		}
		else
		{
			LCD_SCI_CLR;
		}
		val=val<<1;
		LCD_SCL_SET;
	}
#endif
}


void write_dot(char dx,int dy,int color)
{
	LCD_WR_CMD(XS,dx>>8); // Column address start2
	LCD_WR_CMD(XS+1,dx); // Column address start1
	LCD_WR_CMD(YS,dy>>8); // Row address start2
	LCD_WR_CMD(YS+1,dy); // Row address start1
        
	LCD_WR_CMD(0x22,color);
}


void loadFrameBuffer_ave()
{
    int  xsize=640, ysize=480;
    unsigned char *buffer;
    FILE *infile=fopen("/dev/fb0","rb");
    long fp;
    int i,j,k;
    unsigned long offset=0;
    int p;
	int r1,g1,b1;
	int r,g,b;
	long minsum=0;
	long nowsum=0;
    int flag;
	int ra,ga,ba;
    int drawmap[2][ysize/2][xsize/2];
    int diffmap[ysize/2][xsize/2];
    int diffsx, diffsy, diffex, diffey;
    int numdiff=0;
    int area;
    
	buffer = (unsigned char *) malloc(xsize * ysize * 2);
    fseek(infile, 0, 0);
    
    if (fread (buffer, xsize * ysize *2, sizeof(unsigned char), infile) != 1)
        printf ("Read < %d chars when loading file %s\n", hsize*vsize*3, "ss");
    
	LCD_WR_CMD(XS,0x0000); // Column address start2
	LCD_WR_CMD(XS+1,0x0000); // Column address start1
	LCD_WR_CMD(XE,0x0000); // Column address end2
	LCD_WR_CMD(XE+1,0x00EF); // Column address end1
	LCD_WR_CMD(YS,0x0000); // Row address start2
	LCD_WR_CMD(YS+1,0x0000); // Row address start1
	LCD_WR_CMD(YE,0x0001); // Row address end2
	LCD_WR_CMD(YE+1,0x003F); // Row address end1
        
        LCD_WR_REG(0x22);
    LCD_CS_CLR;
    LCD_RS_SET;
    
    for (i=0; i < ysize/2; i++) {
        for(j=0; j< xsize/2; j++) {
            diffmap[i][j]=1;
            drawmap[0][i][j]=0;
            LCD_WR_Data(0);
            drawmap[1][i][j]=255;
        }
    }
    
    flag=1;
    
    while (1) {
        
        numdiff=0;
        flag=1-flag;
        diffex=diffey=0;
        diffsx=diffsy=65535;
        
        for(i=0; i < ysize; i+=2){
            for(j=0; j < xsize; j+=2) {
                offset =  (i * xsize+ j)*2;
                p=(buffer[offset+1] << 8) | buffer[offset];
                r = (p & RGB565_MASK_RED) >> 11;
                g = (p & RGB565_MASK_GREEN) >> 5;
                b = (p & RGB565_MASK_BLUE);
                
                r <<= 1;
                b <<= 1;
                
                offset = ( (i+1) * xsize +j )*2;
                p=(buffer[offset+1] << 8) | buffer[offset];
                r1 = (p & RGB565_MASK_RED) >> 11;
                g1 = (p & RGB565_MASK_GREEN) >> 5;
                b1 = (p & RGB565_MASK_BLUE);
                
                r += r1<<1;
                g += g1;
                b += b1 <<1;
                
                offset = ( i*xsize + j+1)*2;
                p=(buffer[offset+1] << 8) | buffer[offset];
                r1 = (p & RGB565_MASK_RED) >> 11;
                g1 = (p & RGB565_MASK_GREEN) >> 5;
                b1 = (p & RGB565_MASK_BLUE);
                
                r += r1<<1;
                g += g1;
                b += b1 <<1;
                
                offset=((i+1)*xsize + j+1)*2;
                p=(buffer[offset+1] << 8) | buffer[offset];
                r1 = (p & RGB565_MASK_RED) >> 11;
                g1 = (p & RGB565_MASK_GREEN) >> 5;
                b1 = (p & RGB565_MASK_BLUE);
                
                r += r1<<1;
                g += g1;
                b += b1 <<1;
                
                p=RGB565(r, g, b);
                
                //drawmap[flag][i>>1][j>>1] = p;
                if (drawmap[1-flag][i>>1][j>>1] != p) {
                    drawmap[flag][i>>1][j>>1] = p;
                    diffmap[i>>1][j>>1]=1;
                    drawmap[1-flag][i>>1][j>>1]=p;
                    numdiff++;
                    if ((i>>1) < diffsx)
                        diffsx = i>>1;
                    if ((i>>1) > diffex)
                        diffex = i >> 1;
                    if ((j>>1)< diffsy)
                        diffsy=j>>1;
                    if ((j>>1)>diffey)
                        diffey = j >>1;
                    
                } else {
                    diffmap[i>>1][j>>1]=0;
                }
            }
            
        }
        if (numdiff > 10){
           // printf ("(%d, %d) - (%d, %d)\n",diffsx, diffsy, diffex, diffey);
            
            area = ((abs(diffex - diffsx)+1)*(1+abs(diffey-diffsy)));
          //  printf("diff:%d, area:%d, cov:%f\n",numdiff, area,(1.0*numdiff)/area);
        }
        if (numdiff< 1000){
            for (i=diffsx; i<=diffex; i++){
                for (j=diffsy;j<=diffey; j++) {
                    if (diffmap[i][j]!=0)
                        write_dot(i,j,drawmap[flag][i][j]);
                }
            }
        } else{
       		LCD_WR_CMD(XS,diffsx>>8); // Column address start2
		LCD_WR_CMD(XS+1,diffsx); // Column address start1
		LCD_WR_CMD(XE,diffex>>8); // Column address end2
		LCD_WR_CMD(XE+1,diffex); // Column address end1
		LCD_WR_CMD(YS,diffsy>>8); // Row address start2
		LCD_WR_CMD(YS+1,diffsy); // Row address start1
		LCD_WR_CMD(YE,diffey>>8); // Row address end2
		LCD_WR_CMD(YE+1,diffey); // Row address end1

            LCD_WR_REG(0x22);
            LCD_CS_CLR;
            LCD_RS_SET;
            //printf ("(%d, %d) - (%d, %d)\n",diffsx, diffsy, diffex, diffey);
            for (i=diffsx; i<=diffex; i++){
                for (j=diffsy;j<=diffey; j++) {
                    LCD_WR_Data(drawmap[flag][i][j]);
                }
            }
        }
        
        fseek(infile, 0, 0);
        
        if (fread (buffer, xsize * ysize *2, sizeof(unsigned char), infile) != 1)
            printf ("Read < %d chars when loading file %s\n", hsize*vsize*3, "ss");
    }
}

void loadFrameBuffer_diff()
{
    int  xsize=640, ysize=480;
    unsigned char *buffer;
    FILE *infile=fopen("/dev/fb0","rb");
    long fp;
    int i,j,k;
    unsigned long offset=0;
    int p;
	int r1,g1,b1;
	int r,g,b;
	long minsum=0;
	long nowsum=0;
    int flag;
	int ra,ga,ba;
    int drawmap[2][ysize/2][xsize/2];
    int diffmap[ysize/2][xsize/2];
    int diffsx, diffsy, diffex, diffey;
    int numdiff=0;
    int area;
    
	buffer = (unsigned char *) malloc(xsize * ysize * 2);
    fseek(infile, 0, 0);
    
    if (fread (buffer, xsize * ysize *2, sizeof(unsigned char), infile) != 1)
        printf ("Read < %d chars when loading file %s\n", hsize*vsize*3, "ss");
    
       		LCD_WR_CMD(XS,diffsx>>8); // Column address start2
		LCD_WR_CMD(XS+1,diffsx); // Column address start1
		LCD_WR_CMD(XE,diffex>>8); // Column address end2
		LCD_WR_CMD(XE+1,diffex); // Column address end1
		LCD_WR_CMD(YS,diffsy>>8); // Row address start2
		LCD_WR_CMD(YS+1,diffsy); // Row address start1
		LCD_WR_CMD(YE,diffey>>8); // Row address end2
		LCD_WR_CMD(YE+1,diffey); // Row address end1
        
        LCD_WR_REG(0x22);
    LCD_CS_CLR;
    LCD_RS_SET;
    
    for (i=0; i < ysize/2; i++) {
        for(j=0; j< xsize/2; j++) {
            diffmap[i][j]=1;
            drawmap[0][i][j]=0;
            LCD_WR_Data(0);
            drawmap[1][i][j]=255;
        }
    }

    flag=1;
    
    while (1) {
        
        numdiff=0;
        flag=1-flag;
        diffex=diffey=0;
        diffsx=diffsy=65535;

        for(i=0; i < ysize; i+=2){
            for(j=0; j < xsize; j+=2) {
                offset =  (i * xsize+ j)*2;
                p=(buffer[offset+1] << 8) | buffer[offset];
                r = (p & RGB565_MASK_RED) >> 11;
                g = (p & RGB565_MASK_GREEN) >> 5;
                b = (p & RGB565_MASK_BLUE);
                
                r <<= 1;
                b <<= 1;
                
                offset = ( (i+1) * xsize +j )*2;
                p=(buffer[offset+1] << 8) | buffer[offset];
                r1 = (p & RGB565_MASK_RED) >> 11;
                g1 = (p & RGB565_MASK_GREEN) >> 5;
                b1 = (p & RGB565_MASK_BLUE);
                
                r += r1<<1;
                g += g1;
                b += b1 <<1;
                
                offset = ( i*xsize + j+1)*2;
                p=(buffer[offset+1] << 8) | buffer[offset];
                r1 = (p & RGB565_MASK_RED) >> 11;
                g1 = (p & RGB565_MASK_GREEN) >> 5;
                b1 = (p & RGB565_MASK_BLUE);
                
                r += r1<<1;
                g += g1;
                b += b1 <<1;
                
                offset=((i+1)*xsize + j+1)*2;
                p=(buffer[offset+1] << 8) | buffer[offset];
                r1 = (p & RGB565_MASK_RED) >> 11;
                g1 = (p & RGB565_MASK_GREEN) >> 5;
                b1 = (p & RGB565_MASK_BLUE);
                
                r += r1<<1;
                g += g1;
                b += b1 <<1;
                
                p=RGB565(r, g, b);
                
                //drawmap[flag][i>>1][j>>1] = p;
                if (drawmap[1-flag][i>>1][j>>1] != p) {
                    drawmap[flag][i>>1][j>>1] = p;
                    diffmap[i>>1][j>>1]=1;
                    drawmap[1-flag][i>>1][j>>1]=p;
                    numdiff++;
                    if ((i>>1) < diffsx)
                        diffsx = i>>1;
                    if ((i>>1) > diffex)
                        diffex = i >> 1;
                    if ((j>>1)< diffsy)
                        diffsy=j>>1;
                    if ((j>>1)>diffey)
                        diffey = j >>1;
                    
                } else {
                    diffmap[i>>1][j>>1]=0;
                }
            }
            
        }
        if (numdiff > 10){
           // printf ("(%d, %d) - (%d, %d)\n",diffsx, diffsy, diffex, diffey);
            
            area = ((abs(diffex - diffsx)+1)*(1+abs(diffey-diffsy)));
            //printf("diff:%d, area:%d, cov:%f\n",numdiff, area,(1.0*numdiff)/area);
        }
        if (numdiff< 1000){
		LCD_WR_CMD(XE,0x0000); // Column address end2
		LCD_WR_CMD(XE+1,0x00EF); // Column address end1
		LCD_WR_CMD(YE,0x0001); // Row address end2
		LCD_WR_CMD(YE+1,0x003F); // Row address end1
            for (i=diffsx; i<=diffex; i++){
                for (j=diffsy;j<=diffey; j++) {
                    if (diffmap[i][j]!=0)
                        write_dot(i,j,drawmap[flag][i][j]);
                }
            }
            usleep(700L);
            
        } else{
		LCD_WR_CMD(XS,diffsx>>8); // Column address start2
		LCD_WR_CMD(XS+1,diffsx); // Column address start1
		LCD_WR_CMD(XE,diffex>>8); // Column address end2
		LCD_WR_CMD(XE+1,diffex); // Column address end1
		LCD_WR_CMD(YS,diffsy>>8); // Row address start2
		LCD_WR_CMD(YS+1,diffsy); // Row address start1
		LCD_WR_CMD(YE,diffey>>8); // Row address end2
		LCD_WR_CMD(YE+1,diffey); // Row address end1
            LCD_WR_REG(0x22);
            LCD_CS_CLR;
            LCD_RS_SET;
            //printf ("(%d, %d) - (%d, %d)\n",diffsx, diffsy, diffex, diffey);
            for (i=diffsx; i<=diffex; i++){
                for (j=diffsy;j<=diffey; j++) {
                    LCD_WR_Data(drawmap[flag][i][j]);
                }
            }
        }
        
        fseek(infile, 0, 0);
        
        if (fread (buffer, xsize * ysize *2, sizeof(unsigned char), infile) != 1)
            printf ("Read < %d chars when loading file %s\n", hsize*vsize*3, "ss");
    }
}


void loadFrameBuffer_diff_320()
{
    int  xsize=320, ysize=240;
    unsigned char *buffer;
    FILE *infile=fopen("/dev/fb0","rb");
    long fp;
    int i,j,k;
    unsigned long offset=0;
    int p;
    int r1,g1,b1;
    int r,g,b;
    long minsum=0;
    long nowsum=0;
    int flag;
    int ra,ga,ba;
    int drawmap[2][ysize][xsize];
    int diffmap[ysize][xsize];
    int diffsx, diffsy, diffex, diffey;
    int numdiff=0;
    int area;
    
    buffer = (unsigned char *) malloc(xsize * ysize * 2);
    fseek(infile, 0, 0);
    
    if (fread (buffer, xsize * ysize *2, sizeof(unsigned char), infile) != 1)
        printf ("Read < %d chars when loading file %s\n", hsize*vsize*3, "ss");
    
    LCD_WR_CMD(XS,diffsx>>8); // Column address start2
    LCD_WR_CMD(XS+1,diffsx); // Column address start1
    LCD_WR_CMD(XE,diffex>>8); // Column address end2
    LCD_WR_CMD(XE+1,diffex); // Column address end1
    LCD_WR_CMD(YS,diffsy>>8); // Row address start2
    LCD_WR_CMD(YS+1,diffsy); // Row address start1
    LCD_WR_CMD(YE,diffey>>8); // Row address end2
    LCD_WR_CMD(YE+1,diffey); // Row address end1
        
    LCD_WR_REG(0x22);
    LCD_CS_CLR;
    LCD_RS_SET;
    
    for (i=0; i < ysize; i++) {
        for(j=0; j< xsize; j++) {
            diffmap[i][j]=1;
            drawmap[0][i][j]=0;
            LCD_WR_Data(0);
            drawmap[1][i][j]=255;
        }
    }
    
    flag=1;
    
    while (1) {
        
        numdiff=0;
        flag=1-flag;
        diffex=diffey=0;
        diffsx=diffsy=65535;
        
        for(i=0; i < ysize; i++){
            for(j=0; j < xsize; j++) {
               offset =  (i * xsize+ j)*2;
                p=(buffer[offset+1] << 8) | buffer[offset];
                
                //drawmap[flag][i>>1][j>>1] = p;
                if (drawmap[1-flag][i][j] != p) {
                    drawmap[flag][i][j] = p;
                    diffmap[i][j]=1;
                    drawmap[1-flag][i][j]=p;
                    numdiff++;
                    if ((i) < diffsx)
                        diffsx = i;
                    if ((i) > diffex)
                        diffex = i ;
                    if ((j)< diffsy)
                        diffsy=j;
                    if ((j)>diffey)
                        diffey = j ;
                    
                } else {
                    diffmap[i][j]=0;
                }
               // offset++;
            }
            
        }
        if (numdiff > 400){
            // printf ("(%d, %d) - (%d, %d)\n",diffsx, diffsy, diffex, diffey);
            
            area = ((abs(diffex - diffsx)+1)*(1+abs(diffey-diffsy)));
            //printf("diff:%d, area:%d, cov:%f\n",numdiff, area,(1.0*numdiff)/area);
        }
        if (numdiff< 400){
            for (i=diffsx; i<=diffex; i++){
                for (j=diffsy;j<=diffey; j++) {
                    if (diffmap[i][j]!=0)
                        write_dot(i,j,drawmap[flag][i][j]);
                }
            }
            usleep(700L);
            
        } else{
       		LCD_WR_CMD(XS,diffsx>>8); // Column address start2
		LCD_WR_CMD(XS+1,diffsx); // Column address start1
		LCD_WR_CMD(XE,diffex>>8); // Column address end2
		LCD_WR_CMD(XE+1,diffex); // Column address end1
		LCD_WR_CMD(YS,diffsy>>8); // Row address start2
		LCD_WR_CMD(YS+1,diffsy); // Row address start1
		LCD_WR_CMD(YE,diffey>>8); // Row address end2
		LCD_WR_CMD(YE+1,diffey); // Row address end1
            LCD_WR_REG(0x22);
            LCD_CS_CLR;
            LCD_RS_SET;
            
            //printf ("(%d, %d) - (%d, %d)\n",diffsx, diffsy, diffex, diffey);
            for (i=diffsx; i<=diffex; i++){
                for (j=diffsy;j<=diffey; j++) {
                    LCD_WR_Data(drawmap[flag][i][j]);
                }
            }
        }
        
        fseek(infile, 0, 0);
        
        if (fread (buffer, xsize * ysize *2, sizeof(unsigned char), infile) != 1)
            printf ("Read < %d chars when loading file %s\n", hsize*vsize*3, "ss");
    }
}

void LCD_Init()
{
	LCD_RST_CLR;
	delay (100);
	LCD_RST_SET;
	delay (100);
    
	LCD_WR_CMD(0x0046,0x0091);
	LCD_WR_CMD(0x0047,0x0011);
	LCD_WR_CMD(0x0048,0x0000);
	LCD_WR_CMD(0x0049,0x0066);
	LCD_WR_CMD(0x004A,0x0037);
	LCD_WR_CMD(0x004B,0x0004);
	LCD_WR_CMD(0x004C,0x0011);
	LCD_WR_CMD(0x004D,0x0077);
	LCD_WR_CMD(0x004E,0x0000);
	LCD_WR_CMD(0x004F,0x001F);
	LCD_WR_CMD(0x0050,0x000F);
	LCD_WR_CMD(0x0051,0x0000);
	//240x320 window setting
	LCD_WR_CMD(0x0002,0x0000); // Column address start2
	LCD_WR_CMD(0x0003,0x0000); // Column address start1
	LCD_WR_CMD(0x0004,0x0000); // Column address end2
	LCD_WR_CMD(0x0005,0x00EF); // Column address end1
	LCD_WR_CMD(0x0006,0x0000); // Row address start2
	LCD_WR_CMD(0x0007,0x0000); // Row address start1
	LCD_WR_CMD(0x0008,0x0001); // Row address end2
	LCD_WR_CMD(0x0009,0x003F); // Row address end1
	// Display Setting
	LCD_WR_CMD(0x0001,0x0006); // IDMON=0, INVON=1, NORON=1, PTLON=0
	LCD_WR_CMD(0x0016,0x0068); // MY=0, MX=0, MV=0, ML=1, BGR=0, TEON=0
	LCD_WR_CMD(0x0023,0x0095); // N_DC=1001 0101
	LCD_WR_CMD(0x0024,0x0095); // PI_DC=1001 0101
	LCD_WR_CMD(0x0025,0x00ff); // I_DC=1111 1111
	LCD_WR_CMD(0x0027,0x0002); // N_BP=0000 0010
	LCD_WR_CMD(0x0028,0x0002); // N_FP=0000 0010
	LCD_WR_CMD(0x0029,0x0002); // PI_BP=0000 0010
	LCD_WR_CMD(0x002A,0x0002); // PI_FP=0000 0010
	LCD_WR_CMD(0x002C,0x0002); // I_BP=0000 0010
	LCD_WR_CMD(0x002D,0x0002); // I_FP=0000 0010
	LCD_WR_CMD(0x003A,0x00A1); // N_RTN=0000, N_NW=001
	LCD_WR_CMD(0x003B,0x0001); // PI_RTN=0000, PI_NW=001
	LCD_WR_CMD(0x003C,0x00F0); // I_RTN=1111, I_NW=000
	LCD_WR_CMD(0x003D,0x0000); // DIV=00
	delay(2);
	LCD_WR_CMD(0x0035,0x0038); // EQS=38h
	LCD_WR_CMD(0x0036,0x0078); // EQP=78h
	LCD_WR_CMD(0x003E,0x0038); // SON=38h
	LCD_WR_CMD(0x0040,0x000F); // GDON=0Fh
	LCD_WR_CMD(0x0041,0x00F0); // GDOFF
	// Power Supply Setting
	LCD_WR_CMD(0x0019,0x0047); // CADJ=0100, CUADJ=100, OSD_EN=1 ,60Hz
	LCD_WR_CMD(0x0093,0x000F); // RADJ=1111, 100%
	delay(1);
	LCD_WR_CMD(0x0020,0x0050); // BT=0100
	LCD_WR_CMD(0x001D,0x0007); // VC1=111
	LCD_WR_CMD(0x001E,0x0000); // VC3=000
	LCD_WR_CMD(0x001F,0x0001); // VRH=0011
	LCD_WR_CMD(0x0044,0x0034); // VCM=11 0100
	LCD_WR_CMD(0x0045,0x0010); // VDV=1 0000
	LCD_WR_CMD(0x0057,0x0002); // Test Mode enable 
	LCD_WR_CMD(0x0055,0x0000); // VDC_SL=000, VDDD=1.95V 
	LCD_WR_CMD(0x0057,0x0000); // Test Mode disable 
	delay(1);
	LCD_WR_CMD(0x001C,0x0004); // AP=100
	delay(2);
	LCD_WR_CMD(0x0043,0x0080); //set VCOMG=1
	delay(1);
	LCD_WR_CMD(0x001B,0x0008); // GASENB=0, PON=0, DK=1, XDK=0, VLCD_TRI=0, STB=0
	delay(4);
	LCD_WR_CMD(0x001B,0x0010); // GASENB=0, PON=1, DK=0, XDK=0, VLCD_TRI=0, STB=0
	delay(4);
	// Display ON Setting
	LCD_WR_CMD(0x0090,0x007F); // SAP=0111 1111
	LCD_WR_CMD(0x0026,0x0004); //GON=0, DTE=0, D=01
	delay(1);
	LCD_WR_CMD(0x0026,0x0024); //GON=1, DTE=0, D=01
	delay(1);
	LCD_WR_CMD(0x0026,0x002C); //GON=1, DTE=0, D=11
	delay(1);
	LCD_WR_CMD(0x0026,0x003C); //GON=1, DTE=1, D=11
	delay(5);}

void LCD_test()
{
	int temp,num,i;
	char n;
    
	LCD_WR_CMD(XS,0x0000); // Column address start2
	LCD_WR_CMD(XS+1,0x0000); // Column address start1
	LCD_WR_CMD(XE,0x0000); // Column address end2
	LCD_WR_CMD(XE+1,0x00EF); // Column address end1
	LCD_WR_CMD(YS,0x0000); // Row address start2
	LCD_WR_CMD(YS+1,0x0000); // Row address start1
	LCD_WR_CMD(YE,0x0001); // Row address end2
	LCD_WR_CMD(YE+1,0x003F); // Row address end1
    
	LCD_WR_REG(0x22);
	LCD_CS_CLR;
	LCD_RS_SET;
	for(n=0;n<8;n++)
	{
	    temp=color[n];
		for(num=40*240;num>0;num--)
		{
			LCD_WR_Data(temp);
		}
	}
	for(n=0;n<8;n++)
	{
	LCD_WR_CMD(XS,0x0000); // Column address start2
	LCD_WR_CMD(XS+1,0x0000); // Column address start1
	LCD_WR_CMD(XE,0x0000); // Column address end2
	LCD_WR_CMD(XE+1,0x00EF); // Column address end1
	LCD_WR_CMD(YS,0x0000); // Row address start2
	LCD_WR_CMD(YS+1,0x0000); // Row address start1
	LCD_WR_CMD(YE,0x0001); // Row address end2
	LCD_WR_CMD(YE+1,0x003F); // Row address end1
        
		LCD_WR_REG(0x22);
		LCD_CS_CLR;
		LCD_RS_SET;
	    temp=color[n];
		for(i=0;i<240;i++)
		{
			for(num=0;num<320;num++)
			{
		  		LCD_WR_Data(temp);
			}
		}
	}
	LCD_CS_SET;
}

int main (void)
{
    printf("bcm2835 init now\n");
    if (!bcm2835_init())
    {
        printf("bcm2835 init error\n");
        return 1;
    }
    bcm2835_gpio_fsel(SPICS, BCM2835_GPIO_FSEL_OUTP) ;
    bcm2835_gpio_fsel(SPIRS, BCM2835_GPIO_FSEL_OUTP) ;
    bcm2835_gpio_fsel(SPIRST, BCM2835_GPIO_FSEL_OUTP) ;
    bcm2835_gpio_fsel(LCDPWM, BCM2835_GPIO_FSEL_OUTP) ;
    
#ifdef BCM2708SPI
    bcm2835_spi_begin();
    bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);
    bcm2835_spi_setDataMode(BCM2835_SPI_MODE3);
    bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_2);
    
    //bcm2835_spi_chipSelect(BCM2835_SPI_CS1);
    //bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS1,LOW);
#else
    bcm2835_gpio_fsel(SPISCL, BCM2835_GPIO_FSEL_OUTP) ;
    bcm2835_gpio_fsel(SPISCI, BCM2835_GPIO_FSEL_OUTP) ;
#endif
    LCD_PWM_CLR;
    printf ("Raspberry Pi MZTX28H LCD Testing...\n") ;
    printf ("http://jwlcd-tp.taobao.com\n") ;
    
    LCD_Init();
    LCD_test();
    //loadFrameBuffer_diff_320();
    loadFrameBuffer_diff();
    return 0 ;
}