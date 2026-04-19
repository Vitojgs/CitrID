	//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
	//           INSTITUTO POLIT�CNICO DO C�VADO E DO AVE
	//                          2022/2023
	//             ENGENHARIA DE SISTEMAS INFORM�TICOS
	//                    VIS�O POR COMPUTADOR
	//
	//             [   ]
	//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

	// Desabilita (no MSVC++) warnings de fun��es n�o seguras (fopen, sscanf, etc...)
	#define _CRT_SECURE_NO_WARNINGS

	#include <stdio.h>
	#include <ctype.h>
	#include <string.h>
	#include <malloc.h>
	#include <math.h>
	#include "vc.h"
	#ifndef MAX
	#define MAX(a,b) (((a)>(b))?(a):(b))
	#endif
	
	//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
	//            FUN��ES: ALOCAR E LIBERTAR UMA IMAGEM
	//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


	// Alocar mem�ria para uma imagem
	IVC *vc_image_new(int width, int height, int channels, int levels)
	{
		IVC *image = (IVC *) malloc(sizeof(IVC));

		if(image == NULL) return NULL;
		if((levels <= 0) || (levels > 255)) return NULL;

		image->width = width;
		image->height = height;
		image->channels = channels;
		image->levels = levels;
		image->bytesperline = image->width * image->channels;
		image->data = (unsigned char *) malloc(image->width * image->height * image->channels * sizeof(char));

		if(image->data == NULL)
		{
			return vc_image_free(image);
		}

		return image;
	}


	// Libertar mem�ria de uma imagem
	IVC *vc_image_free(IVC *image)
	{
		if(image != NULL)
		{
			if(image->data != NULL)
			{
				free(image->data);
				image->data = NULL;
			}

			free(image);
			image = NULL;
		}

		return image;
	}


	//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
	//    FUN��ES: LEITURA E ESCRITA DE IMAGENS (PBM, PGM E PPM)
	//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


	char *netpbm_get_token(FILE *file, char *tok, int len)
	{
		char *t;
		int c;
		
		for(;;)
		{
			while(isspace(c = getc(file)));
			if(c != '#') break;
			do c = getc(file);
			while((c != '\n') && (c != EOF));
			if(c == EOF) break;
		}
		
		t = tok;
		
		if(c != EOF)
		{
			do
			{
				*t++ = c;
				c = getc(file);
			} while((!isspace(c)) && (c != '#') && (c != EOF) && (t - tok < len - 1));
			
			if(c == '#') ungetc(c, file);
		}
		
		*t = 0;
		
		return tok;
	}


	long int unsigned_char_to_bit(unsigned char *datauchar, unsigned char *databit, int width, int height)
	{
		int x, y;
		int countbits;
		long int pos, counttotalbytes;
		unsigned char *p = databit;

		*p = 0;
		countbits = 1;
		counttotalbytes = 0;

		for(y=0; y<height; y++)
		{
			for(x=0; x<width; x++)
			{
				pos = width * y + x;

				if(countbits <= 8)
				{
					// Numa imagem PBM:
					// 1 = Preto
					// 0 = Branco
					//*p |= (datauchar[pos] != 0) << (8 - countbits);
					
					// Na nossa imagem:
					// 1 = Branco
					// 0 = Preto
					*p |= (datauchar[pos] == 0) << (8 - countbits);

					countbits++;
				}
				if((countbits > 8) || (x == width - 1))
				{
					p++;
					*p = 0;
					countbits = 1;
					counttotalbytes++;
				}
			}
		}

		return counttotalbytes;
	}


	void bit_to_unsigned_char(unsigned char *databit, unsigned char *datauchar, int width, int height)
	{
		int x, y;
		int countbits;
		long int pos;
		unsigned char *p = databit;

		countbits = 1;

		for(y=0; y<height; y++)
		{
			for(x=0; x<width; x++)
			{
				pos = width * y + x;

				if(countbits <= 8)
				{
					// Numa imagem PBM:
					// 1 = Preto
					// 0 = Branco
					//datauchar[pos] = (*p & (1 << (8 - countbits))) ? 1 : 0;

					// Na nossa imagem:
					// 1 = Branco
					// 0 = Preto
					datauchar[pos] = (*p & (1 << (8 - countbits))) ? 0 : 1;
					
					countbits++;
				}
				if((countbits > 8) || (x == width - 1))
				{
					p++;
					countbits = 1;
				}
			}
		}
	}


	IVC *vc_read_image(char *filename)
	{
		FILE *file = NULL;
		IVC *image = NULL;
		unsigned char *tmp;
		char tok[20];
		long int size, sizeofbinarydata;
		int width, height, channels;
		int levels = 255;
		int v;
		
		// Abre o ficheiro
		if((file = fopen(filename, "rb")) != NULL)
		{
			// Efectua a leitura do header
			netpbm_get_token(file, tok, sizeof(tok));

			if(strcmp(tok, "P4") == 0) { channels = 1; levels = 1; }	// Se PBM (Binary [0,1])
			else if(strcmp(tok, "P5") == 0) channels = 1;				// Se PGM (Gray [0,MAX(level,255)])
			else if(strcmp(tok, "P6") == 0) channels = 3;				// Se PPM (RGB [0,MAX(level,255)])
			else
			{
				#ifdef VC_DEBUG
				printf("ERROR -> vc_read_image():\n\tFile is not a valid PBM, PGM or PPM file.\n\tBad magic number!\n");
				#endif

				fclose(file);
				return NULL;
			}
			
			if(levels == 1) // PBM
			{
				if(sscanf(netpbm_get_token(file, tok, sizeof(tok)), "%d", &width) != 1 || 
				sscanf(netpbm_get_token(file, tok, sizeof(tok)), "%d", &height) != 1)
				{
					#ifdef VC_DEBUG
					printf("ERROR -> vc_read_image():\n\tFile is not a valid PBM file.\n\tBad size!\n");
					#endif

					fclose(file);
					return NULL;
				}

				// Aloca mem�ria para imagem
				image = vc_image_new(width, height, channels, levels);
				if(image == NULL) return NULL;

				sizeofbinarydata = (image->width / 8 + ((image->width % 8) ? 1 : 0)) * image->height;
				tmp = (unsigned char *) malloc(sizeofbinarydata);
				if(tmp == NULL) return 0;

				#ifdef VC_DEBUG
				printf("\nchannels=%d w=%d h=%d levels=%d\n", image->channels, image->width, image->height, levels);
				#endif

				if((v = fread(tmp, sizeof(unsigned char), sizeofbinarydata, file)) != sizeofbinarydata)
				{
					#ifdef VC_DEBUG
					printf("ERROR -> vc_read_image():\n\tPremature EOF on file.\n");
					#endif

					vc_image_free(image);
					fclose(file);
					free(tmp);
					return NULL;
				}

				bit_to_unsigned_char(tmp, image->data, image->width, image->height);

				free(tmp);
			}
			else // PGM ou PPM
			{
				if(sscanf(netpbm_get_token(file, tok, sizeof(tok)), "%d", &width) != 1 || 
				sscanf(netpbm_get_token(file, tok, sizeof(tok)), "%d", &height) != 1 || 
				sscanf(netpbm_get_token(file, tok, sizeof(tok)), "%d", &levels) != 1 || levels <= 0 || levels > 255)
				{
					#ifdef VC_DEBUG
					printf("ERROR -> vc_read_image():\n\tFile is not a valid PGM or PPM file.\n\tBad size!\n");
					#endif

					fclose(file);
					return NULL;
				}

				// Aloca mem�ria para imagem
				image = vc_image_new(width, height, channels, levels);
				if(image == NULL) return NULL;

				#ifdef VC_DEBUG
				printf("\nchannels=%d w=%d h=%d levels=%d\n", image->channels, image->width, image->height, levels);
				#endif

				size = image->width * image->height * image->channels;

				if((v = fread(image->data, sizeof(unsigned char), size, file)) != size)
				{
					#ifdef VC_DEBUG
					printf("ERROR -> vc_read_image():\n\tPremature EOF on file.\n");
					#endif

					vc_image_free(image);
					fclose(file);
					return NULL;
				}
			}
			
			fclose(file);
		}
		else
		{
			#ifdef VC_DEBUG
			printf("ERROR -> vc_read_image():\n\tFile not found.\n");
			#endif
		}
		
		return image;
	}


	int vc_write_image(const char *filename, IVC *image)
	{
		FILE *file = NULL;
		unsigned char *tmp;
		long int totalbytes, sizeofbinarydata;
		
		if(image == NULL) return 0;

		if((file = fopen(filename, "wb")) != NULL)
		{
			if(image->levels == 1)
			{
				sizeofbinarydata = (image->width / 8 + ((image->width % 8) ? 1 : 0)) * image->height + 1;
				tmp = (unsigned char *) malloc(sizeofbinarydata);
				if(tmp == NULL) return 0;
				
				fprintf(file, "%s %d %d\n", "P4", image->width, image->height);
				
				totalbytes = unsigned_char_to_bit(image->data, tmp, image->width, image->height);
				printf("Total = %ld\n", totalbytes);
				if(fwrite(tmp, sizeof(unsigned char), totalbytes, file) != totalbytes)
				{
					#ifdef VC_DEBUG
					fprintf(stderr, "ERROR -> vc_read_image():\n\tError writing PBM, PGM or PPM file.\n");
					#endif

					fclose(file);
					free(tmp);
					return 0;
				}

				free(tmp);
			}
			else
			{
				fprintf(file, "%s %d %d 255\n", (image->channels == 1) ? "P5" : "P6", image->width, image->height);
			
				if(fwrite(image->data, image->bytesperline, image->height, file) != image->height)
				{
					#ifdef VC_DEBUG
					fprintf(stderr, "ERROR -> vc_read_image():\n\tError writing PBM, PGM or PPM file.\n");
					#endif

					fclose(file);
					return 0;
				}
			}
			
			fclose(file);

			return 1;
		}
		
		return 0;
	}

	// Extrai o canal vermelho e coloca-o em tons de cinza
	int vc_rgb_get_red_gray(IVC *src) {
		if (src == NULL || src->data == NULL || src->channels != 3) return 0;

		int size = src->width * src->height;
		unsigned char *data = src->data;

		for (int i = 0; i < size; i++) {
			unsigned char r = data[i * 3];     // Pega no Vermelho
			data[i * 3] = r;                 // R = R
			data[i * 3 + 1] = r;             // G = R
			data[i * 3 + 2] = r;             // B = R
		}
		return 1;
	}

	// Extrai o canal verde e coloca-o em tons de cinza
	int vc_rgb_get_green_gray(IVC *src) {
		if (src == NULL || src->data == NULL || src->channels != 3) return 0;

		int size = src->width * src->height;
		unsigned char *data = src->data;

		for (int i = 0; i < size; i++) {
			unsigned char g = data[i * 3 + 1]; // Pega no Verde
			data[i * 3] = g;                 // R = G
			data[i * 3 + 1] = g;             // G = G
			data[i * 3 + 2] = g;             // B = G
		}
		return 1;
	}

	// Extrai o canal azul e coloca-o em tons de cinza
	int vc_rgb_get_blue_gray(IVC *src) {
		if (src == NULL || src->data == NULL || src->channels != 3) return 0;

		int size = src->width * src->height;
		unsigned char *data = src->data;

		for (int i = 0; i < size; i++) {
			unsigned char b = data[i * 3 + 2]; // Pega no Azul
			data[i * 3] = b;                 // R = B
			data[i * 3 + 1] = b;             // G = B
			data[i * 3 + 2] = b;             // B = B
		}
		return 1;
	}

	int vc_rgb_to_hsv_2(IVC *src, IVC *dst)
{
	unsigned char *datasrc = (unsigned char *)src->data;
	unsigned char *datadst = (unsigned char *)dst->data;
	int width = src->width;
	int height = src->height;
	int bytesperline = src->bytesperline;
	int channels = src->channels;
	float r, g, b, hue, saturation, value;
	float rgb_max, rgb_min;
	int i, size;

	// Verificação de erros
	if ((src->width <= 0) || (src->height <= 0) || (src->data == NULL)) return 0;
	if ((src->width != dst->width) || (src->height != dst->height) || (src->channels != dst->channels)) return 0;
	if (channels != 3) return 0;

	size = width * height * channels;

	for (i = 0; i<size; i = i + channels)
	{
		r = (float)datasrc[i];
		g = (float)datasrc[i + 1];
		b = (float)datasrc[i + 2];

		// Calcula valores m ximo e m nimo dos canais de cor R, G e B
		rgb_max = (float)(r > g ? (r > b ? r : b) : (g > b ? g : b));
		rgb_min = (float)(r < g ? (r < b ? r : b) : (g < b ? g : b));

		// Value toma valores entre [0,255]
		value = rgb_max;
		if (value == 0.0)
		{
			hue = 0.0;
			saturation = 0.0;
		}
		else
		{
			// Saturation toma valores entre [0,255]
			saturation = ((rgb_max - rgb_min) / rgb_max) * (float) 255.0;

			if (saturation == 0.0)
			{
				hue = 0.0;
			}
			else
			{
				// R, G e B tomam valores entre [0,1]
				r /= 255.0;
				g /= 255.0;
				b /= 255.0;

				// Calcula valores m ximo e m nimo dos canais de cor R, G e B (tomam valores entre [0,1])
				rgb_max = (r > g ? (r > b ? r : b) : (g > b ? g : b));
				rgb_min = (r < g ? (r < b ? r : b) : (g < b ? g : b));

				// Hue toma valores entre [0,360]
				if ((rgb_max == r) && (g >= b))
				{
					hue = 60 * (g - b) / (rgb_max - rgb_min);
				}
				else if ((rgb_max == r) && (b > g))
				{
					hue = 360 + 60 * (g - b) / (rgb_max - rgb_min);
				}
				else if (rgb_max == g)
				{
					hue = 120 + 60 * (b - r) / (rgb_max - rgb_min);
				}
				else /* rgb_max == b*/
				{
					hue = 240 + 60 * (r - g) / (rgb_max - rgb_min);
				}
			}
		}

		// Atribui valores entre [0,255]
		datadst[i] = (unsigned char)(hue / 360.0 * 255.0);
		datadst[i + 1] = (unsigned char)(saturation);
		datadst[i + 2] = (unsigned char)(value);
	}

	return 1;
}

int vc_hsv_segmentation(IVC *src, IVC *dst, int hmin, int hmax, int smin, int smax, int vmin, int vmax){
	unsigned char *datasrc = (unsigned char *)src->data;
	unsigned char *datadst = (unsigned char *)dst->data;
	int width = src->width;
	int height = src->height;
	int bytesperline = src->bytesperline;
	int channels = src->channels;
	int i, size;
	int hmin_deg, hmax_deg, hmin_255, hmax_255;
	int hue_ok, sv_ok;

	// Verificação de erros
	if ((src->width <= 0) || (src->height <= 0) || (src->data == NULL)) return 0;
	if ((src->width != dst->width) || (src->height != dst->height) || (src->channels != dst->channels)) return 0;
	if (channels != 3) return 0;

	// Hue entra em graus [0,360], mas no pixel HSV está em [0,255].
	// Normaliza e converte para comparar diretamente com datasrc[i].
	hmin_deg = hmin;
	hmax_deg = hmax;
	if (hmin_deg < 0) hmin_deg = 0;
	if (hmin_deg > 360) hmin_deg = 360;
	if (hmax_deg < 0) hmax_deg = 0;
	if (hmax_deg > 360) hmax_deg = 360;
	hmin_255 = (hmin_deg * 255) / 360;
	hmax_255 = (hmax_deg * 255) / 360;

	size = width * height * channels;

	for (i = 0; i<size; i = i + channels)
	{
		// Se hmax < hmin, o intervalo de Hue atravessa 360->0 (zona do vermelho).
		if (hmin_255 <= hmax_255)
		{
			hue_ok = (datasrc[i] >= hmin_255) && (datasrc[i] <= hmax_255);
		}
		else
		{
			hue_ok = (datasrc[i] >= hmin_255) || (datasrc[i] <= hmax_255);
		}

		sv_ok = (datasrc[i + 1] >= smin) && (datasrc[i + 1] <= smax) &&
				(datasrc[i + 2] >= vmin) && (datasrc[i + 2] <= vmax);

		if (hue_ok && sv_ok)
		{
			datadst[i] = 255; //datasrc[i];
			datadst[i + 1] = 255; //datasrc[i + 1];
			datadst[i + 2] = 255; //datasrc[i + 2];
		}
		else
		{
			datadst[i] = 0;
			datadst[i + 1] = 0;
			datadst[i + 2] = 0;
		}
	}

	return 1;
}

int vc_scale_gray_to_rgb(IVC *src, IVC *dst)
{
    unsigned char *datasrc, *datadst;
    int x, y;

    if (src == NULL || dst == NULL) return 0;
    if (src->data == NULL || dst->data == NULL) return 0;

    // src: 1 canal (grayscale), dst: 3 canais (RGB)
    if (src->channels != 1) return 0;
    if (dst->channels != 3) return 0;	

    // Mesma dimensão
    if (src->width != dst->width || src->height != dst->height) return 0;

    // 8-bit
    if (src->levels != 255 || dst->levels != 255) return 0;

    datasrc = (unsigned char *)src->data;
    datadst = (unsigned char *)dst->data;

    for (y = 0; y < src->height; y++)
    {
        for (x = 0; x < src->width; x++)
        {
            int pos_src = y * src->bytesperline + x;
            int pos_dst = y * dst->bytesperline + x * 3;

            unsigned char gray = datasrc[pos_src]; // 0..255
            unsigned char r, g, b;

            // Colormap tipo "rainbow/jet" em 4 segmentos (0..255)
            // 0..63:    azul -> ciano
            // 64..127:  ciano -> verde
            // 128..191: verde -> amarelo
            // 192..255: amarelo -> vermelho
            if (gray < 64)
            {
                // azul (0,0,255) -> ciano (0,255,255)
                r = 0;
                g = (unsigned char)(gray * 255 / 63);   // 0..252
                b = 255;
            }
            else if (gray < 128)
            {
                // ciano (0,255,255) -> verde (0,255,0)
                r = 0;
                g = 255;
                b = (unsigned char)(255 - (gray - 64) * 255 / 63); // 255..3
            }
            else if (gray < 192)
            {
                // verde (0,255,0) -> amarelo (255,255,0)
                r = (unsigned char)((gray - 128) * 255 / 63); // 0..252
                g = 255;
                b = 0;
            }
            else
            {
                // amarelo (255,255,0) -> vermelho (255,0,0)
                r = 255;
                g = (unsigned char)(255 - (gray - 192) * 4); // 255..3
                b = 0;
            }

            datadst[pos_dst + 0] = r;   // R
            datadst[pos_dst + 1] = g; // G
            datadst[pos_dst + 2] = b;   // B
        }
    }

    return 1;
}

int vc_scale_gray_to_rgb2(IVC *src, IVC *dst) // Mais eficiente a partir de ~350pixeis
{
    unsigned char *datasrc, *datadst;
    int x, y;

    // LUTs (0..255)
    unsigned char red[256], green[256], blue[256];
    int i;
	int size = dst->width * dst->height;

    if (src == NULL || dst == NULL) return 0;
    if (src->data == NULL || dst->data == NULL) return 0;

    // src: 1 canal (grayscale), dst: 3 canais (RGB)
    if (src->channels != 1) return 0;
    if (dst->channels != 3) return 0;

    // Mesma dimensão
    if (src->width != dst->width || src->height != dst->height) return 0;

    // 8-bit
    if (src->levels != 255 || dst->levels != 255) return 0;

    // -----------------------------
    // 1) Construir o colormap (LUT)
    // -----------------------------
    for (i = 0; i < 256; i++)
    {
        if (i < 64)
        {
            // azul (0,0,255) -> ciano (0,255,255)
            red[i]   = 0;
            green[i] = (unsigned char)(i * 255 / 63);   // 0..255
            blue[i]  = 255;
        }
        else if (i < 128)
        {
            // ciano (0,255,255) -> verde (0,255,0)
            red[i]   = 0;
            green[i] = 255;
            blue[i]  = (unsigned char)(255 - (i - 64) * 255 / 63); // 255..0
        }
        else if (i < 192)
        {
            // verde (0,255,0) -> amarelo (255,255,0)
            red[i]   = (unsigned char)((i - 128) * 255 / 63); // 0..255
            green[i] = 255;
            blue[i]  = 0;
        }
        else
        {
            // amarelo (255,255,0) -> vermelho (255,0,0)
            red[i]   = 255;
            green[i] = (unsigned char)(255 - (i - 192) * 255 / 63); // 255..0
            blue[i]  = 0;
        }
    }

    datasrc = (unsigned char *)src->data;
    datadst = (unsigned char *)dst->data;

    // ---------------------------------
    // 2) Aplicar LUT a todos os pixels
    // ---------------------------------
    for (i=0; i<size; i++)
	{
		unsigned char gray = datasrc[i]; // 0..255

		datadst[i * 3 + 0] = red[gray];   // R
		datadst[i * 3 + 1] = green[gray]; // G
		datadst[i * 3 + 2] = blue[gray];  // B
	}

    return 1;
}

int vc_pet_activity(IVC *image)
{
    int x, y;
    int red = 0, yellow = 0, green = 0, blue = 0;
    int brain_pixels = 0;

    int pos;
    unsigned char *data = image->data;

    for(y = 0; y < image->height; y++)
    {
        for(x = 0; x < image->width; x++)
        {
            pos = (y * image->width + x) * image->channels;

            int r = data[pos];
            int g = data[pos + 1];
            int b = data[pos + 2];

            float rf = r / 255.0;
            float gf = g / 255.0;
            float bf = b / 255.0;

            float max = fmax(rf, fmax(gf, bf));
            float min = fmin(rf, fmin(gf, bf));
            float delta = max - min;

            float h, s, v;

            /* Value */
            v = max;

            /* Saturation */
            if(max == 0)
                s = 0;
            else
                s = delta / max;

            /* Hue */
            if(delta == 0)
                h = 0;
            else if(max == rf)
                h = 60 * fmod((gf - bf) / delta, 6);
            else if(max == gf)
                h = 60 * ((bf - rf) / delta + 2);
            else
                h = 60 * ((rf - gf) / delta + 4);

            if(h < 0) h += 360;

            /* converter para percentagem */
            s = s * 100;
            v = v * 100;

            /* considerar apenas pixels do cérebro */
            if(s >= 50 && v >= 50)
            {
                brain_pixels++;

                if((h >= 0 && h <= 45) || (h >= 291 && h <= 360))
                    red++;

                else if(h >= 46 && h <= 70)
                    yellow++;

                else if(h >= 71 && h <= 160)
                    green++;

                else if(h >= 161 && h <= 290)
                    blue++;
            }
        }
    }

    printf("Numero de pixeis vermelhos: %d\n", red);
    printf("Numero de pixeis amarelos: %d\n", yellow);
    printf("Numero de pixeis verdes: %d\n", green);
    printf("Numero de pixeis azuis: %d\n", blue);
    printf("Numero de pixeis do cerebro: %d\n\n", brain_pixels);

    printf("Percentagem de atividade ate 25%%: %.2f\n", (blue * 100.0) / brain_pixels);
    printf("Percentagem de atividade ate 50%%: %.2f\n", (green * 100.0) / brain_pixels);
    printf("Percentagem de atividade ate 75%%: %.2f\n", (yellow * 100.0) / brain_pixels);
    printf("Percentagem de atividade ate 100%%: %.2f\n", (red * 100.0) / brain_pixels);

    return 1;
}


int vc_gray_to_binary(IVC *src, IVC *dst, int threshold)
{
    int x, y;
    long int pos;
    unsigned char *datasrc = NULL;
    unsigned char *datadst = NULL;

    // Verificação de erros
    if ((src == NULL) || (dst == NULL)) return 0;
    if ((src->data == NULL) || (dst->data == NULL)) return 0;
    if ((src->width <= 0) || (src->height <= 0)) return 0;
    if ((src->width != dst->width) || (src->height != dst->height)) return 0;
    if ((src->channels != 1) || (dst->channels != 1)) return 0;

    datasrc = (unsigned char *)src->data;
    datadst = (unsigned char *)dst->data;

    for (y = 0; y < src->height; y++)
    {
        for (x = 0; x < src->width; x++)
        {
            pos = y * src->bytesperline + x * src->channels;

            if (datasrc[pos] > threshold)
                datadst[pos] = 255;
            else
                datadst[pos] = 0;
        }
    }

    return 1;
}

int vc_gray_to_binary_range(IVC *src, IVC *dst, int thresholdmin, int thresholdmax, int mode)
{
    int x, y;
    long int pos;
    unsigned char *datasrc = NULL;
    unsigned char *datadst = NULL;

    if (src == NULL || dst == NULL) return 0;
    if (src->data == NULL || dst->data == NULL) return 0;
    if (src->width <= 0 || src->height <= 0) return 0;
    if (src->width != dst->width || src->height != dst->height) return 0;
    if (src->channels != 1 || dst->channels != 1) return 0;
    if (thresholdmin < 0 || thresholdmin > 255) return 0;
    if (thresholdmax < 0 || thresholdmax > 255) return 0;
    if (thresholdmin > thresholdmax) return 0;
    if (mode != 0 && mode != 1) return 0;

    datasrc = (unsigned char *) src->data;
    datadst = (unsigned char *) dst->data;

    for (y = 0; y < src->height; y++)
    {
        for (x = 0; x < src->width; x++)
        {
            pos = y * src->bytesperline + x * src->channels;

            if (mode == 1)
            {
                // Branco se estiver entre thresholdmin e thresholdmax
                if (datasrc[pos] >= thresholdmin && datasrc[pos] <= thresholdmax)
                    datadst[pos] = 255;
                else
                    datadst[pos] = 0;
            }
            else
            {
                // Branco se estiver fora do intervalo
                if (datasrc[pos] < thresholdmin || datasrc[pos] > thresholdmax)
                    datadst[pos] = 255;
                else
                    datadst[pos] = 0;
            }
        }
    }

    return 1;
}




/* int kernel = 25;
int offset = (kernel-1)/2;
*/
int vc_gray_to_binary_midpoint(IVC *src, IVC *dst, int kernel)
{
    int x, y, kx, ky;
    int offset = (kernel - 1) / 2;
    long int pos, posk;
    unsigned char *datasrc = (unsigned char *)src->data;
    unsigned char *datadst = (unsigned char *)dst->data;

    int min, max, threshold;

    for (y = 0; y < src->height; y++)
    {
        for (x = 0; x < src->width; x++)
        {
            pos = y * src->bytesperline + x * src->channels;
            min = 255;
            max = 0;

            for (ky = -offset; ky <= offset; ky++)
            {
                for (kx = -offset; kx <= offset; kx++)
                {
                    int xx = x + kx;
                    int yy = y + ky;

                    if (xx >= 0 && xx < src->width && yy >= 0 && yy < src->height)
                    {
                        posk = yy * src->bytesperline + xx * src->channels;

                        if (datasrc[posk] < min) min = datasrc[posk];
                        if (datasrc[posk] > max) max = datasrc[posk];
                    }
                }
            }

            threshold = (min + max) / 2;

            

            if (datasrc[pos] > threshold)
                datadst[pos] = 255;
            else
                datadst[pos] = 0;
        }
    }

    return 1;
}

int vc_gray_to_binary_bersen(IVC *src, IVC *dst, int kernel)
{
    int x, y, kx, ky, Cmin;
    int offset = (kernel - 1) / 2;
    long int pos, posk;
    unsigned char *datasrc = (unsigned char *)src->data;
    unsigned char *datadst = (unsigned char *)dst->data;

    int min, max, threshold;

    for (y = 0; y < src->height; y++)
    {
        for (x = 0; x < src->width; x++)
        {
            pos = y * src->bytesperline + x * src->channels;
            min = 255;
            max = 0;
           

            for (ky = -offset; ky <= offset; ky++)
            {
                for (kx = -offset; kx <= offset; kx++)
                {
                    int xx = x + kx;
                    int yy = y + ky;

                    if (xx >= 0 && xx < src->width && yy >= 0 && yy < src->height)
                    {
                        posk = yy * src->bytesperline + xx * src->channels;

                        if (datasrc[posk] < min) min = datasrc[posk];
                        if (datasrc[posk] > max) max = datasrc[posk];
                    }
                }
            }

             if ((max - min) < Cmin)
            {
                threshold = 255 / 2; 
            }
            else
            {
                threshold = (min + max) / 2;
            }

            
            if (datasrc[pos] > threshold)
                datadst[pos] = 255;
            else
                datadst[pos] = 0;
        }
    }

    return 1;
}

int vc_gray_to_binary_niblack(IVC *src, IVC *dst, int kernel, float k)
{
    int x, y, kx, ky;
    int offset = (kernel - 1) / 2;
    long int pos, posk;
    unsigned char *datasrc = (unsigned char *)src->data;
    unsigned char *datadst = (unsigned char *)dst->data;

    int count;
    float soma, soma2, med, desvio, threshold;

    for (y = 0; y < src->height; y++)
    {
        for (x = 0; x < src->width; x++)
        {
            soma = 0;
            soma2 = 0;
            count = 0;

            
            for (ky = -offset; ky <= offset; ky++)
            {
                for (kx = -offset; kx <= offset; kx++)
                {
                    int xx = x + kx;
                    int yy = y + ky;

                    if (xx >= 0 && xx < src->width && yy >= 0 && yy < src->height)
                    {
                        posk = yy * src->bytesperline + xx * src->channels;
                        unsigned char pixel = datasrc[posk];

                        soma += pixel;
                        soma += pixel * pixel;
                        count++;
                    }
                }
            }

            // Média
            med = soma / count;

            // Desvio padrão
            desvio = sqrt((soma2 / count) - (med * med));

            // Threshold Niblack
            threshold = med + k * desvio;

            pos = y * src->bytesperline + x * src->channels;

            if (datasrc[pos] > threshold)
                datadst[pos] = 255;
            else
                datadst[pos] = 0;
        }
    }

    return 1;
}


int vc_binary_erode(IVC *src, IVC *dst, int kernel)
{
    int x, y, kx, ky;
    int offset = (kernel - 1) / 2;
    long int pos, posk;
    unsigned char *datasrc = (unsigned char *)src->data;
    unsigned char *datadst = (unsigned char *)dst->data;

    for (y = 0; y < src->height; y++)
    {
        for (x = 0; x < src->width; x++)
        {
            int all_white = 1;

            for (ky = -offset; ky <= offset && all_white; ky++)
            {
                for (kx = -offset; kx <= offset; kx++)
                {
                    int xx = x + kx;
                    int yy = y + ky;

                    if (xx >= 0 && xx < src->width && yy >= 0 && yy < src->height)
                    {
                        posk = yy * src->bytesperline + xx * src->channels;

                        if (datasrc[posk] == 0)
                        {
                            all_white = 0;
                            break;
                        }
                    }
                    else
                    {
                        
                        all_white = 0;
                        break;
                    }
                }
            }

            pos = y * src->bytesperline + x * src->channels;

            if (all_white)
                datadst[pos] = 255;  
            else
                datadst[pos] = 0;    
        }
    }

    return 1;
}

int vc_binary_dilate(IVC *src, IVC *dst, int kernel)
{
    int x, y, kx, ky;
    int offset = (kernel - 1) / 2;
    long int pos, posk;
    unsigned char *datasrc = (unsigned char *)src->data;
    unsigned char *datadst = (unsigned char *)dst->data;

    for (y = 0; y < src->height; y++)
    {
        for (x = 0; x < src->width; x++)
        {
            int found = 0;

            for (ky = -offset; ky <= offset && !found; ky++)
            {
                for (kx = -offset; kx <= offset; kx++)
                {
                    int xx = x + kx;
                    int yy = y + ky;

                    if (xx >= 0 && xx < src->width && yy >= 0 && yy < src->height)
                    {
                        posk = yy * src->bytesperline + xx * src->channels;

                        if (datasrc[posk] == 255)
                        {
                            found = 1;
                            break;
                        }
                    }
                }
            }

            pos = y * src->bytesperline + x * src->channels;

            if (found)
                datadst[pos] = 255;  
            else
                datadst[pos] = 0;    
        }
    }

    return 1;
}
// Abertura = Erosão → Dilatação  (remove ruído/estruturas finas)
int vc_binary_open(IVC *src, IVC *dst, int kernel)
{
    IVC *tmp = vc_image_new(src->width, src->height, src->channels, src->levels);
    if (tmp == NULL) return 0;

    vc_binary_erode(src, tmp, kernel);
    vc_binary_dilate(tmp, dst, kernel);

    vc_image_free(tmp);
    return 1;
}
// Fecho = Dilatação → Erosão  (preenche os buracos internos)
int vc_binary_close(IVC *src, IVC *dst, int kernel)
{
    IVC *tmp = vc_image_new(src->width, src->height, src->channels, src->levels);
    if (tmp == NULL) return 0;

    vc_binary_dilate(src, tmp, kernel);
    vc_binary_erode(tmp, dst, kernel);

    vc_image_free(tmp);
    return 1;
}

int vc_gray_mask(IVC *src, IVC *mask, IVC *dst) //func necessaria acho ex2brain
{
    int x, y;
    long int pos;
    unsigned char *datasrc, *datamask, *datadst;

    // Validação
    if (src == NULL || mask == NULL || dst == NULL) return 0;
    if (src->data == NULL || mask->data == NULL || dst->data == NULL) return 0;
    if (src->width != mask->width || src->height != mask->height) return 0;
    if (src->width != dst->width || src->height != dst->height) return 0;
    if (src->channels != 1 || mask->channels != 1 || dst->channels != 1) return 0;

    datasrc = src->data;
    datamask = mask->data;
    datadst = dst->data;

    for (y = 0; y < src->height; y++)
    {
        for (x = 0; x < src->width; x++)
        {
            pos = y * src->bytesperline + x;

            if (datamask[pos] == 255)
                datadst[pos] = datasrc[pos]; // mantém pixel original
            else
                datadst[pos] = 0; // apaga (fundo)
        }
    }

    return 1;
}

/* int vc_brain_segment(IVC *src, IVC *dst)
{
    IVC *th = NULL;
    IVC *er = NULL;
    IVC *di = NULL;
    IVC *cl = NULL;

    if (src == NULL || dst == NULL) return 0;
    if (src->data == NULL || dst->data == NULL) return 0;
    if (src->channels != 1 || dst->channels != 1) return 0;
    if (src->width != dst->width || src->height != dst->height) return 0;

    th = vc_image_new(src->width, src->height, 1, 255);
    er = vc_image_new(src->width, src->height, 1, 255);
    di = vc_image_new(src->width, src->height, 1, 255);
    cl = vc_image_new(src->width, src->height, 1, 255);

    if (th == NULL || er == NULL || di == NULL || cl == NULL)
    {
        vc_image_free(th);
        vc_image_free(er);
        vc_image_free(di);
        vc_image_free(cl);
        return 0;
    }

    // 1) Threshold inicial
    vc_gray_to_binary(src, th, 25);
    vc_write_image("01_threshold.pgm", th);

    // 2) Erosão forte para remover o crânio (estrutura fina)
    vc_binary_erode(th, er, 11);
    vc_write_image("02_erode.pgm", er);

    // 3) Dilatação para recuperar a forma do cérebro
    vc_binary_dilate(er, di, 11);
    vc_write_image("03_dilate.pgm", di);

    // 4) Fecho para preencher pequenas falhas internas
    vc_binary_close(di, cl, 5);
    vc_write_image("04_close.pgm", cl);

    // 5) Aplicar a máscara final à imagem original
    vc_gray_mask(src, cl, dst);
    vc_write_image("05_result.pgm", dst);

    vc_image_free(th);
    vc_image_free(er);
    vc_image_free(di);
    vc_image_free(cl);

    return 1;
}
*/
// Etiquetagem de blobs
// src		: Imagem bin�ria de entrada
// dst		: Imagem grayscale (ir� conter as etiquetas)
// nlabels	: Endere�o de mem�ria de uma vari�vel, onde ser� armazenado o n�mero de etiquetas encontradas.
// OVC*		: Retorna um array de estruturas de blobs (objectos), com respectivas etiquetas. � necess�rio libertar posteriormente esta mem�ria.
OVC* vc_binary_blob_labelling(IVC *src, IVC *dst, int *nlabels)
{
	unsigned char *datasrc = (unsigned char *)src->data;
	unsigned char *datadst = (unsigned char *)dst->data;
	int width = src->width;
	int height = src->height;
	int bytesperline = src->bytesperline;
	int channels = src->channels;
	int x, y, a, b;
	long int i, size;
	long int posX, posA, posB, posC, posD;
	int labeltable[256] = { 0 };
	int labelarea[256] = { 0 };
	int label = 1; // Etiqueta inicial.
	int num, tmplabel;
	OVC *blobs; // Apontador para array de blobs (objectos) que ser� retornado desta fun��o.

	// Verifica��o de erros
	if ((src->width <= 0) || (src->height <= 0) || (src->data == NULL)) return 0;
	if ((src->width != dst->width) || (src->height != dst->height) || (src->channels != dst->channels)) return NULL;
	if (channels != 1) return NULL;

	// Copia dados da imagem bin�ria para imagem grayscale
	memcpy(datadst, datasrc, bytesperline * height);

	// Todos os pix�is de plano de fundo devem obrigat�riamente ter valor 0
	// Todos os pix�is de primeiro plano devem obrigat�riamente ter valor 255
	// Ser�o atribu�das etiquetas no intervalo [1,254]
	// Este algoritmo est� assim limitado a 254 labels
	for (i = 0, size = bytesperline * height; i<size; i++)
	{
		if (datadst[i] != 0) datadst[i] = 255;
	}

	// Limpa os rebordos da imagem bin�ria
	for (y = 0; y<height; y++)
	{
		datadst[y * bytesperline + 0 * channels] = 0;
		datadst[y * bytesperline + (width - 1) * channels] = 0;
	}
	for (x = 0; x<width; x++)
	{
		datadst[0 * bytesperline + x * channels] = 0;
		datadst[(height - 1) * bytesperline + x * channels] = 0;
	}

	// Efectua a etiquetagem
	for (y = 1; y<height - 1; y++)
	{
		for (x = 1; x<width - 1; x++)
		{
			// Kernel:
			// A B C
			// D X

			posA = (y - 1) * bytesperline + (x - 1) * channels; // A
			posB = (y - 1) * bytesperline + x * channels; // B
			posC = (y - 1) * bytesperline + (x + 1) * channels; // C
			posD = y * bytesperline + (x - 1) * channels; // D
			posX = y * bytesperline + x * channels; // X

			// Se o pixel foi marcado
			if (datadst[posX] != 0)
			{
				if ((datadst[posA] == 0) && (datadst[posB] == 0) && (datadst[posC] == 0) && (datadst[posD] == 0))
				{
					datadst[posX] = label;
					labeltable[label] = label;
					label++;
				}
				else
				{
					num = 255;

					// Se A est� marcado
					if (datadst[posA] != 0) num = labeltable[datadst[posA]];
					// Se B est� marcado, e � menor que a etiqueta "num"
					if ((datadst[posB] != 0) && (labeltable[datadst[posB]] < num)) num = labeltable[datadst[posB]];
					// Se C est� marcado, e � menor que a etiqueta "num"
					if ((datadst[posC] != 0) && (labeltable[datadst[posC]] < num)) num = labeltable[datadst[posC]];
					// Se D est� marcado, e � menor que a etiqueta "num"
					if ((datadst[posD] != 0) && (labeltable[datadst[posD]] < num)) num = labeltable[datadst[posD]];

					// Atribui a etiqueta ao pixel
					datadst[posX] = num;
					labeltable[num] = num;

					// Actualiza a tabela de etiquetas
					if (datadst[posA] != 0)
					{
						if (labeltable[datadst[posA]] != num)
						{
							for (tmplabel = labeltable[datadst[posA]], a = 1; a<label; a++)
							{
								if (labeltable[a] == tmplabel)
								{
									labeltable[a] = num;
								}
							}
						}
					}
					if (datadst[posB] != 0)
					{
						if (labeltable[datadst[posB]] != num)
						{
							for (tmplabel = labeltable[datadst[posB]], a = 1; a<label; a++)
							{
								if (labeltable[a] == tmplabel)
								{
									labeltable[a] = num;
								}
							}
						}
					}
					if (datadst[posC] != 0)
					{
						if (labeltable[datadst[posC]] != num)
						{
							for (tmplabel = labeltable[datadst[posC]], a = 1; a<label; a++)
							{
								if (labeltable[a] == tmplabel)
								{
									labeltable[a] = num;
								}
							}
						}
					}
					if (datadst[posD] != 0)
					{
						if (labeltable[datadst[posD]] != num)
						{
							for (tmplabel = labeltable[datadst[posD]], a = 1; a<label; a++)
							{
								if (labeltable[a] == tmplabel)
								{
									labeltable[a] = num;
								}
							}
						}
					}
				}
			}
		}
	}

	// Volta a etiquetar a imagem
	for (y = 1; y<height - 1; y++)
	{
		for (x = 1; x<width - 1; x++)
		{
			posX = y * bytesperline + x * channels; // X

			if (datadst[posX] != 0)
			{
				datadst[posX] = labeltable[datadst[posX]];
			}
		}
	}

	//printf("\nMax Label = %d\n", label);

	// Contagem do n�mero de blobs
	// Passo 1: Eliminar, da tabela, etiquetas repetidas
	for (a = 1; a<label - 1; a++)
	{
		for (b = a + 1; b<label; b++)
		{
			if (labeltable[a] == labeltable[b]) labeltable[b] = 0;
		}
	}
	// Passo 2: Conta etiquetas e organiza a tabela de etiquetas, para que n�o hajam valores vazios (zero) entre etiquetas
	*nlabels = 0;
	for (a = 1; a<label; a++)
	{
		if (labeltable[a] != 0)
		{
			labeltable[*nlabels] = labeltable[a]; // Organiza tabela de etiquetas
			(*nlabels)++; // Conta etiquetas
		}
	}

	// Se n�o h� blobs
	if (*nlabels == 0) return NULL;

	// Cria lista de blobs (objectos) e preenche a etiqueta
	blobs = (OVC *)calloc((*nlabels), sizeof(OVC));
	if (blobs != NULL)
	{
		for (a = 0; a<(*nlabels); a++) blobs[a].label = labeltable[a];
	}
	else return NULL;

	return blobs;
}


int vc_binary_blob_info(IVC *src, OVC *blobs, int nblobs)
{
	unsigned char *data = (unsigned char *)src->data;
	int width = src->width;
	int height = src->height;
	int bytesperline = src->bytesperline;
	int channels = src->channels;
	int x, y, i;
	long int pos;
	int xmin, ymin, xmax, ymax;
	long int sumx, sumy;

	// Verifica��o de erros
	if ((src->width <= 0) || (src->height <= 0) || (src->data == NULL)) return 0;
	if (channels != 1) return 0;

	// Conta �rea de cada blob
	for (i = 0; i<nblobs; i++)
	{
		xmin = width - 1;
		ymin = height - 1;
		xmax = 0;
		ymax = 0;

		sumx = 0;
		sumy = 0;

		blobs[i].area = 0;

		for (y = 1; y<height - 1; y++)
		{
			for (x = 1; x<width - 1; x++)
			{
				pos = y * bytesperline + x * channels;

				if (data[pos] == blobs[i].label)
				{
					// �rea
					blobs[i].area++;

					// Centro de Gravidade
					sumx += x;
					sumy += y;

					// Bounding Box
					if (xmin > x) xmin = x;
					if (ymin > y) ymin = y;
					if (xmax < x) xmax = x;
					if (ymax < y) ymax = y;

					// Per�metro
					// Se pelo menos um dos quatro vizinhos n�o pertence ao mesmo label, ent�o � um pixel de contorno
					if ((data[pos - 1] != blobs[i].label) || (data[pos + 1] != blobs[i].label) || (data[pos - bytesperline] != blobs[i].label) || (data[pos + bytesperline] != blobs[i].label))
					{
						blobs[i].perimeter++;
					}
				}
			}
		}

		// Bounding Box
		blobs[i].x = xmin;
		blobs[i].y = ymin;
		blobs[i].width = (xmax - xmin) + 1;
		blobs[i].height = (ymax - ymin) + 1;

		// Centro de Gravidade
		//blobs[i].xc = (xmax - xmin) / 2;
		//blobs[i].yc = (ymax - ymin) / 2;
		blobs[i].xc = sumx / MAX(blobs[i].area, 1);
		blobs[i].yc = sumy / MAX(blobs[i].area, 1);
	}

	return 1;
}


int vc_gray_histogram_show(IVC *src, IVC *dst)
{
    unsigned char *datasrc = (unsigned char *)src->data;
    unsigned char *datadst = (unsigned char *)dst->data;
    int i, x, y;
    int ni[256] = { 0 };
    float pdf[256];
    float max_pdf = 0;

    if ((src == NULL) || (dst == NULL)) return 0;
    if ((src->data == NULL) || (dst->data == NULL)) return 0;
    if (src->channels != 1 || dst->channels != 1) return 0;
    if (dst->width != 256) return 0;

    for (i = 0; i < (src->width * src->height); i++)
    {
        ni[datasrc[i]]++;
    }

    for (i = 0; i < 256; i++)
    {
        pdf[i] = (float)ni[i] / (float)(src->width * src->height);
        if (pdf[i] > max_pdf) max_pdf = pdf[i];
    }

    memset(datadst, 0, dst->width * dst->height);

    for (x = 0; x < 256; x++)
    {
        int bar_height = (int)((pdf[x] / max_pdf) * (dst->height - 1));

        for (y = (dst->height - 1); y >= (dst->height - 1 - bar_height); y--)
        {
            datadst[y * dst->width + x] = 255;
        }
    }

    return 1;
}

int vc_gray_histogram_equalization(IVC *src, IVC *dst)
{
    unsigned char *datasrc = (unsigned char *)src->data;
    unsigned char *datadst = (unsigned char *)dst->data;
    int i;
    float ni[256] = { 0 };
    float pdf[256];
    float cdf[256];
    int n = src->width * src->height;
    int L = 256;

    if ((src == NULL) || (dst == NULL)) return 0;
    if (src->channels != 1 || dst->channels != 1) return 0;
    if (src->width != dst->width || src->height != dst->height) return 0;

    for (i = 0; i < n; i++)
    {
        ni[datasrc[i]]++;
    }

    float sum = 0.0f;
    for (i = 0; i < L; i++)
    {
        pdf[i] = ni[i] / (float)n;
        sum += pdf[i];
        cdf[i] = sum;
    }

    for (i = 0; i < n; i++)
    {
        float val = cdf[datasrc[i]] * (float)(L - 1);
        
        if (val > 255) val = 255;
        if (val < 0) val = 0;

        datadst[i] = (unsigned char) (val + 0.5f); // verificar com o professor se é necessário arredondar
    }

    return 1;
}

int vc_gray_edge_prewitt(IVC *src, IVC *dst, float th)
{
    unsigned char *datasrc = (unsigned char *)src->data;
    unsigned char *datadst = (unsigned char *)dst->data;
    int width = src->width;
    int height = src->height;
    int x, y;
    float mx, my, magn;

    if ((src == NULL) || (dst == NULL) || (src->data == NULL) || (dst->data == NULL)) return 0;
    if ((width != dst->width) || (height != dst->height) || (src->channels != 1) || (dst->channels != 1)) return 0;

    memset(datadst, 0, width * height * sizeof(unsigned char));

    // AJUSTE: Normalizar o threshold apenas UMA VEZ antes de entrar no ciclo
    if (th < 1.0f) th *= 255.0f;

    for (y = 1; y < height - 1; y++)
    {
        for (x = 1; x < width - 1; x++)
        {
            // Gradiente em X (Mx) -
            mx = (float)datasrc[(y - 1) * width + (x + 1)] - datasrc[(y - 1) * width + (x - 1)] +
                 (float)datasrc[y * width + (x + 1)]       - datasrc[y * width + (x - 1)] +
                 (float)datasrc[(y + 1) * width + (x + 1)] - datasrc[(y + 1) * width + (x - 1)];

            // Gradiente em Y (My) -
            my = (float)datasrc[(y + 1) * width + (x - 1)] + datasrc[(y + 1) * width + x] + datasrc[(y + 1) * width + (x + 1)] -
                 (float)datasrc[(y - 1) * width + (x - 1)] - datasrc[(y - 1) * width + x] - datasrc[(y - 1) * width + (x + 1)];

            // Magnitude -
            magn = sqrtf((mx * mx) + (my * my));

            if (magn > th)
            {
                datadst[y * width + x] = 255;
            }
            else
            {
                datadst[y * width + x] = 0;
            }
        }
    }

    return 1;
}