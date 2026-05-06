#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <malloc.h>
#include <math.h>
#include "vc.h"
#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//            FUNÇÕES: ALOCAR E LIBERTAR UMA IMAGEM
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


// Alocar memória para uma imagem
IVC* vc_image_new(int width, int height, int channels, int levels)
{
    IVC* image = (IVC*)malloc(sizeof(IVC));

    if (image == NULL) return NULL;
    if ((levels <= 0) || (levels > 255)) return NULL;

    image->width = width;
    image->height = height;
    image->channels = channels;
    image->levels = levels;
    image->bytesperline = image->width * image->channels;
    image->data = (unsigned char*)malloc(image->width * image->height * image->channels * sizeof(char));

    if (image->data == NULL)
    {
        return vc_image_free(image);
    }

    return image;
}


// Libertar memória de uma imagem
IVC* vc_image_free(IVC* image)
{
    if (image != NULL)
    {
        if (image->data != NULL)
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
//    FUNÇÕES: LEITURA E ESCRITA DE IMAGENS (PBM, PGM E PPM)
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


char* netpbm_get_token(FILE* file, char* tok, int len)
{
    char* t;
    int c;

    for (;;)
    {
        while (isspace(c = getc(file)));
        if (c != '#') break;
        do c = getc(file);
        while ((c != '\n') && (c != EOF));
        if (c == EOF) break;
    }

    t = tok;

    if (c != EOF)
    {
        do
        {
            *t++ = c;
            c = getc(file);
        } while ((!isspace(c)) && (c != '#') && (c != EOF) && (t - tok < len - 1));

        if (c == '#') ungetc(c, file);
    }

    *t = 0;

    return tok;
}

long int unsigned_char_to_bit(unsigned char* datauchar, unsigned char* databit, int width, int height)
{
    int x, y;
    int countbits;
    long int pos, counttotalbytes;
    unsigned char* p = databit;

    *p = 0;
    countbits = 1;
    counttotalbytes = 0;

    for (y = 0; y < height; y++)
    {
        for (x = 0; x < width; x++)
        {
            pos = width * y + x;

            if (countbits <= 8)
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
            if ((countbits > 8) || (x == width - 1))
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

void bit_to_unsigned_char(unsigned char* databit, unsigned char* datauchar, int width, int height)
{
    int x, y;
    int countbits;
    long int pos;
    unsigned char* p = databit;

    countbits = 1;

    for (y = 0; y < height; y++)
    {
        for (x = 0; x < width; x++)
        {
            pos = width * y + x;

            if (countbits <= 8)
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
            if ((countbits > 8) || (x == width - 1))
            {
                p++;
                countbits = 1;
            }
        }
    }
}

IVC* vc_read_image(char* filename)
{
    FILE* file = NULL;
    IVC* image = NULL;
    unsigned char* tmp;
    char tok[20];
    long int size, sizeofbinarydata;
    int width, height, channels;
    int levels = 255;
    int v;

    // Abre o ficheiro
    if ((file = fopen(filename, "rb")) != NULL)
    {
        // Efectua a leitura do header
        netpbm_get_token(file, tok, sizeof(tok));

        if (strcmp(tok, "P4") == 0) { channels = 1; levels = 1; }	// Se PBM (Binary [0,1])
        else if (strcmp(tok, "P5") == 0) channels = 1;				// Se PGM (Gray [0,MAX(level,255)])
        else if (strcmp(tok, "P6") == 0) channels = 3;				// Se PPM (RGB [0,MAX(level,255)])
        else
        {
#ifdef VC_DEBUG
            printf("ERROR -> vc_read_image():\n\tFile is not a valid PBM, PGM or PPM file.\n\tBad magic number!\n");
#endif

            fclose(file);
            return NULL;
        }

        if (levels == 1) // PBM
        {
            if (sscanf(netpbm_get_token(file, tok, sizeof(tok)), "%d", &width) != 1 ||
                sscanf(netpbm_get_token(file, tok, sizeof(tok)), "%d", &height) != 1)
            {
#ifdef VC_DEBUG
                printf("ERROR -> vc_read_image():\n\tFile is not a valid PBM file.\n\tBad size!\n");
#endif

                fclose(file);
                return NULL;
            }

            // Aloca memória para imagem
            image = vc_image_new(width, height, channels, levels);
            if (image == NULL) return NULL;

            sizeofbinarydata = (image->width / 8 + ((image->width % 8) ? 1 : 0)) * image->height;
            tmp = (unsigned char*)malloc(sizeofbinarydata);
            if (tmp == NULL) return 0;

#ifdef VC_DEBUG
            printf("\nchannels=%d w=%d h=%d levels=%d\n", image->channels, image->width, image->height, levels);
#endif

            if ((v = fread(tmp, sizeof(unsigned char), sizeofbinarydata, file)) != sizeofbinarydata)
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
            if (sscanf(netpbm_get_token(file, tok, sizeof(tok)), "%d", &width) != 1 ||
                sscanf(netpbm_get_token(file, tok, sizeof(tok)), "%d", &height) != 1 ||
                sscanf(netpbm_get_token(file, tok, sizeof(tok)), "%d", &levels) != 1 || levels <= 0 || levels > 255)
            {
#ifdef VC_DEBUG
                printf("ERROR -> vc_read_image():\n\tFile is not a valid PGM or PPM file.\n\tBad size!\n");
#endif

                fclose(file);
                return NULL;
            }

            // Aloca memória para imagem
            image = vc_image_new(width, height, channels, levels);
            if (image == NULL) return NULL;

#ifdef VC_DEBUG
            printf("\nchannels=%d w=%d h=%d levels=%d\n", image->channels, image->width, image->height, levels);
#endif

            size = image->width * image->height * image->channels;

            if ((v = fread(image->data, sizeof(unsigned char), size, file)) != size)
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

int vc_write_image(char* filename, IVC* image)
{
    FILE* file = NULL;
    unsigned char* tmp;
    long int totalbytes, sizeofbinarydata;

    if (image == NULL) return 0;

    if ((file = fopen(filename, "wb")) != NULL)
    {
        if (image->levels == 1)
        {
            sizeofbinarydata = (image->width / 8 + ((image->width % 8) ? 1 : 0)) * image->height + 1;
            tmp = (unsigned char*)malloc(sizeofbinarydata);
            if (tmp == NULL) return 0;

            fprintf(file, "%s %d %d\n", "P4", image->width, image->height);

            totalbytes = unsigned_char_to_bit(image->data, tmp, image->width, image->height);
            printf("Total = %ld\n", totalbytes);
            if (fwrite(tmp, sizeof(unsigned char), totalbytes, file) != totalbytes)
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

            if (fwrite(image->data, image->bytesperline, image->height, file) != image->height)
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

int vc_gray_negative(IVC* srcdst)
{
    if (srcdst == NULL) return 0;
    if (srcdst->channels != 1) return 0;

    IVC* image = srcdst;
    unsigned char* data = (unsigned char*)image->data;
    int width = image->width;
    int height = image->height;
    int channels = (*image).channels;
    int size = height * width;

    for (int i = 0; i < size; i++)
    {
        image->data[i] = 255 - image->data[i];
    }
    printf("Os dados da imagem no formato Graymap foram revertidos para negativo!");
    return 1;
}

int vc_rgb_negative(IVC* srcdst)
{
    if (srcdst == NULL) return 0;

    IVC* image = srcdst;
    unsigned char* data = (unsigned char*)image->data;
    int width = image->width;
    int height = image->height;
    int channels = (*image).channels;
    int size = height * width * channels;

    if (srcdst->channels != 3) return 0;

    for (int i = 0; i < size; i++)
    {
        data[i] = 255 - data[i];
    }
    printf("Os dados da imagem no formato RGB foram revertidos para o seu oposto!");
    return 1;
}

int vc_rgb_get_red_gray(IVC* srcdst)
{
    if (srcdst == NULL) return 0;

    IVC* image = srcdst;

    unsigned char* data = (unsigned char*)image->data;
    int width = image->width;
    int height = image->height;
    int channels = image->channels;
    int bytesperline = image->bytesperline;

    if (image->channels != 3) return 0;

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            // Posição do início do pixel (Red)
            long int pos = (long int)y * bytesperline + x * channels;

            // "Apagamos" o Verde e o Azul
            data[pos + 1] = 0; // Green 
            data[pos + 2] = 0; // Blue 
        }
    }
    return 1;
}

int vc_rgb_get_green_gray(IVC* srcdst)
{
    if (srcdst == NULL) return 0;

    IVC* image = srcdst;

    unsigned char* data = (unsigned char*)image->data;
    int width = image->width;
    int height = image->height;
    int channels = image->channels;
    int bytesperline = image->bytesperline;

    if (image->channels != 3) return 0;

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            // Posição do início do pixel (Green)
            long int pos = (long int)(y * bytesperline) + ((x * channels) + 1);

            // "Apagamos" o Vermelho e o Azul
            data[pos - 1] = 0; // Red 
            data[pos + 1] = 0; // Blue 
        }
    }
    return 1;
}

int vc_rgb_get_blue_gray(IVC* srcdst)
{
    if (srcdst == NULL) return 0;

    IVC* image = srcdst;

    unsigned char* data = (unsigned char*)image->data;
    int width = image->width;
    int height = image->height;
    int channels = image->channels;
    int bytesperline = image->bytesperline;

    if (image->channels != 3) return 0;

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            // Posição do início do pixel (Blue)
            long int pos = (long int)(y * bytesperline) + ((x * channels) + 2);

            // "Apagamos" o Verde e o Vermelho
            data[pos - 2] = 0; // Red
            data[pos - 1] = 0; // Green 
        }
    }
    return 1;
}

int vc_rgb_to_gray(IVC* src, IVC* dst)
{
    if (src == NULL || dst == NULL) return 0;

    unsigned char* datasrc = (unsigned char*)src->data;
    unsigned char* datadst = (unsigned char*)dst->data;
    int width = src->width;
    int height = src->height;
    int bytesperline_src = src->width * src->channels;
    int bytesperline_dst = dst->width * dst->channels;
    int channels_src = src->channels;
    int channels_dst = dst->channels;
    float rf, gf, bf;

    if ((width <= 0) || (height <= 0) || (datasrc == NULL)) return 0;
    if ((width != dst->width) || (height != dst->height)) return 0;
    if ((src->channels != 3) || (dst->channels != 1)) return 0;

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            long int pos_src = (y * bytesperline_src) + (x * channels_src);
            long int pos_dst = (y * bytesperline_dst) + (x * channels_dst);

            rf = datasrc[pos_src];
            gf = datasrc[pos_src + 1];
            bf = datasrc[pos_src + 2];

            datadst[pos_dst] = (unsigned char)((rf * 0.299) + (gf * 0.587) + (bf * 0.114));
        }
    }
    return 1;
}

int vc_rgb_to_hsv(IVC* src, IVC* dst)
{
    unsigned char* datasrc = (unsigned char*)src->data;
    unsigned char* datadst = (unsigned char*)dst->data;
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

    for (i = 0; i < size; i = i + channels)
    {
        r = (float)datasrc[i];
        g = (float)datasrc[i + 1];
        b = (float)datasrc[i + 2];

        // Calcula valores maximo e minimo dos canais de cor R, G e B
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
            saturation = ((rgb_max - rgb_min) / rgb_max) * (float)255.0;

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

int vc_hsv_segmentation(IVC* src, IVC* dst, int hmin, int hmax, int smin, int smax, int vmin, int vmax)
{
    if (src == NULL || dst == NULL) return 0;

    unsigned char* datasrc = (unsigned char*)src->data;
    unsigned char* datadst = (unsigned char*)dst->data;
    int width = src->width;
    int height = src->height;
    int bytesperline_src = src->bytesperline;
    int bytesperline_dst = dst->bytesperline;
    int channels_src = src->channels;
    int channels_dst = dst->channels;
    int hmin_deg, hmax_deg, hmin_255, hmax_255;
    int hue_ok, sv_ok;

    if ((width <= 0) || (height <= 0) || (datasrc == NULL)) return 0;
    if ((src->width != dst->width) || (src->height != dst->height)) return 0;
    if ((channels_src != 3) || (channels_dst != 1)) return 0;

    // Hue entra em graus [0,360], mas no pixel HSV está em [0,255].
    // Normaliza e converte para comparar diretamente com datasrc[i].
    hmin_deg = hmin;
    hmax_deg = hmax;

    if (hmax_deg < 0) hmax_deg = 0;
    if (hmin_deg < 0) hmin_deg = 0;
    if (hmax_deg > 360) hmax_deg = 360;
    if (hmin_deg > 360) hmin_deg = 360;

    hmax_255 = (hmax_deg * 255) / 360;
    hmin_255 = (hmin_deg * 255) / 360;

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            long int pos_src = (long int)(y * bytesperline_src) + (x * channels_src);
            long int pos_dst = (long int)(y * bytesperline_dst) + (x * channels_dst);

            int hue = datasrc[pos_src];
            int sat = datasrc[pos_src + 1];
            int val = datasrc[pos_src + 2];

            if (hmin_255 <= hmax_255)
            {
                hue_ok = (hmax_255 >= hue) && (hmin_255 <= hue);
            }
            else // if it's red, only one of these conditions will be true
            {
                hue_ok = (hmax_255 >= hue) || (hmin_255 <= hue);
            }

            sv_ok = (sat >= smin) && (sat <= smax) &&
                (val >= vmin) && (val <= vmax);

            if (sv_ok && hue_ok)
            {
                datadst[pos_dst] = 255;
            }
            else
            {
                datadst[pos_dst] = 0;
            }
        }
    }
    return 1;
}

int vc_scale_gray_to_rgb(IVC* src, IVC* dst)
{
    if (src == NULL || dst == NULL) return 0;

    unsigned char* datasrc = (unsigned char*)src->data;
    unsigned char* datadst = (unsigned char*)dst->data;
    int width = src->width;
    int height = src->height;
    int bytesperline_src = src->bytesperline;
    int bytesperline_dst = dst->bytesperline;
    int channels_src = src->channels;
    int channels_dst = dst->channels;
    int r = 0;
    int g = 0;
    int b = 0;
    int gray;

    if ((width <= 0) || (height <= 0) || (datasrc == NULL)) return 0;
    if ((width != dst->width) || (height != dst->height)) return 0;
    if ((src->channels != 1) || (dst->channels != 3)) return 0;

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            long int pos_src = (y * bytesperline_src) + (x * channels_src);
            long int pos_dst = (y * bytesperline_dst) + (x * channels_dst);

            gray = datasrc[pos_src];

            if (gray <= 64)
            {
                r = 0;
                g = gray * 4;
                b = 255;
            }
            else if (gray <= 128)
            {
                r = 0;
                g = 255;
                b = 255 - ((gray - 64) * 4);
            }
            else if (gray <= 192)
            {
                r = (gray - 128) * 4;
                g = 255;
                b = 0;
            }
            else if (gray <= 255)
            {
                r = 255;
                g = 255 - ((gray - 192) * 4);
                b = 0;
            }

            r = (r > 255) ? 255 : (r < 0) ? 0 : r;
            g = (g > 255) ? 255 : (g < 0) ? 0 : g;
            b = (b > 255) ? 255 : (b < 0) ? 0 : b;

            datadst[pos_dst] = (unsigned char)r;
            datadst[pos_dst + 1] = (unsigned char)g;
            datadst[pos_dst + 2] = (unsigned char)b;
        }
    }
    return 1;
}

int vc_count_white_pixels(IVC* src)
{
    if (src == NULL) return 0;

    unsigned char* data = (unsigned char*)src->data;
    int width = src->width;
    int height = src->height;
    int bytesperline = src->bytesperline;
    int count = 0;

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            long int pos = (long int)y * bytesperline + x;

            if (data[pos] == 255)
            {
                count++;
            }
        }
    }
    return count;
}

int vc_gray_to_binary(IVC* src, IVC* dst, int threshold)
{
    if (src == NULL || dst == NULL) return 0;

    unsigned char* data_src = (unsigned char*)src->data;
    unsigned char* data_dst = (unsigned char*)dst->data;
    int width = src->width;
    int height = src->height;
    int bytesperline_src = src->bytesperline;
    int bytesperline_dst = dst->bytesperline;
    int channels_src = src->channels;
    int channels_dst = dst->channels;

    if ((width <= 0) || (height <= 0) || (data_src == NULL) || (data_dst == NULL)) return 0;
    if ((width != dst->width) || (height != dst->height)) return 0;
    if ((channels_dst != 1) || (channels_src != 1)) return 0;
    if ((src->levels != 255) || (dst->levels != 255)) return 0;

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            long int pos_src = (long int)(y * bytesperline_src) + x;
            long int pos_dst = (long int)(y * bytesperline_dst) + x;

            if (data_src[pos_src] > threshold)
            {
                data_dst[pos_dst] = 255;
            }
            else
            {
                data_dst[pos_dst] = 0;
            }
        }
    }
    return 1;
}

int vc_gray_to_binary_global_mean(IVC* src, IVC* dst)
{
    if (src == NULL || dst == NULL) return 0;

    unsigned char* data_src = (unsigned char*)src->data;
    unsigned char* data_dst = (unsigned char*)dst->data;
    int width = src->width;
    int height = src->height;
    int bytesperline_src = src->bytesperline;
    int bytesperline_dst = dst->bytesperline;
    int channels_src = src->channels;
    int channels_dst = dst->channels;
    float threshold = 0.0f;
    int size = width * height;
    long int sum = 0;

    if ((width <= 0) || (height <= 0) || (data_src == NULL) || (data_dst == NULL)) return 0;
    if ((width != dst->width) || (height != dst->height)) return 0;
    if ((channels_dst != 1) || (channels_src != 1)) return 0;
    if ((src->levels != 255) || (dst->levels != 2)) return 0;

    for (int i = 0; i < size; i++)
    {
        sum += data_src[i];
    }

    threshold = sum / size;

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            long int pos_src = (long int)(y * bytesperline_src) + x;
            long int pos_dst = (long int)(y * bytesperline_dst) + x;

            if (data_src[pos_src] > threshold)
            {
                data_dst[pos_dst] = 255;
            }
            else
            {
                data_dst[pos_dst] = 0;
            }
        }
    }
    return 1;
}

int vc_gray_to_binary_midpoint(IVC* src, IVC* dst, int kernel)
{
    if (src == NULL || dst == NULL) return 0;

    unsigned char* data_src = (unsigned char*)src->data;
    unsigned char* data_dst = (unsigned char*)dst->data;
    int width = src->width;
    int height = src->height;
    int bytesperline_src = src->bytesperline;
    int bytesperline_dst = dst->bytesperline;
    int channels_src = src->channels;
    int channels_dst = dst->channels;

    if ((width <= 0) || (height <= 0) || (data_src == NULL) || (data_dst == NULL)) return 0;
    if ((width != dst->width) || (height != dst->height)) return 0;
    if ((channels_dst != 1) || (channels_src != 1)) return 0;
    if ((src->levels != 255) || (dst->levels != 2)) return 0;

    int offset = kernel / 2;

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            int vmin = 255; // reset dos valores da vizinhança para cada pixel
            int vmax = 0;

            for (int ky = -offset; ky <= offset; ky++) // isto vai percorrer a vizinhança em todos os pixeis x da linha y
            {
                for (int kx = -offset; kx <= offset; kx++) // offset vai para a posição y e x para ficar ny e nx (vizinhança)
                {
                    int ny = y + ky;
                    int nx = x + kx;

                    if (ny >= 0 && ny < height && nx >= 0 && nx < width)
                    {
                        int pos_vizinho = ny * bytesperline_src + nx;

                        if (data_src[pos_vizinho] < vmin)
                        {
                            vmin = data_src[pos_vizinho];
                        }
                        if (data_src[pos_vizinho] > vmax)
                        {
                            vmax = data_src[pos_vizinho];
                        }
                    }
                }
            }

            int threshold = (vmin + vmax) / 2;
            long int pos_src = (long int)y * bytesperline_src + x;
            long int pos_dst = (long int)y * bytesperline_dst + x;

            if (data_src[pos_src] > threshold)
            {
                data_dst[pos_dst] = 255;
            }
            else
            {
                data_dst[pos_dst] = 0;
            }
        }
    }
    return 1;
}

int vc_gray_to_binary_bernsen(IVC* src, IVC* dst, int kernel)
{
    if (src == NULL || dst == NULL) return 0;

    unsigned char* data_src = (unsigned char*)src->data;
    unsigned char* data_dst = (unsigned char*)dst->data;
    int width = src->width;
    int height = src->height;
    int levels = src->levels;
    int bytesperline_src = src->bytesperline;
    int bytesperline_dst = dst->bytesperline;
    int channels_src = src->channels;
    int channels_dst = dst->channels;
    float threshold = 0.0f;
    int Cmin = 15;

    if ((width <= 0) || (height <= 0) || (data_src == NULL) || (data_dst == NULL)) return 0;
    if ((width != dst->width) || (height != dst->height)) return 0;
    if ((channels_dst != 1) || (channels_src != 1)) return 0;
    if ((src->levels != 255) || (dst->levels != 2)) return 0;

    int offset = kernel / 2;

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            int vmax = 0;
            int vmin = 255;

            for (int ky = -offset; ky <= offset; ky++)
            {
                for (int kx = -offset; kx <= offset; kx++)
                {
                    int pos_vy = y + ky;
                    int pos_vx = x + kx;

                    if (pos_vy >= 0 && pos_vx >= 0 && pos_vy < height && pos_vx < width)
                    {
                        int pos_viz = pos_vy * bytesperline_src + pos_vx;

                        if (data_src[pos_viz] < vmin)
                        {
                            vmin = data_src[pos_viz];
                        }
                        if (data_src[pos_viz] > vmax)
                        {
                            vmax = data_src[pos_viz];
                        }
                    }
                }
            }
            if ((vmax - vmin) < Cmin)
            {
                threshold = (levels / 2);
            }
            else
            {
                threshold = (0.5f) * (vmin + vmax);
            }

            long int pos_src = (y * bytesperline_src) + x;
            long int pos_dst = (y * bytesperline_dst) + x;

            if (data_src[pos_src] > threshold)
            {
                data_dst[pos_dst] = 255;
            }
            else
            {
                data_dst[pos_dst] = 0;
            }
        }
    }
    return 1;
}

int vc_gray_to_binary_niblack(IVC* src, IVC* dst, int kernel, float k)
{
    if (src == NULL || dst == NULL) return 0;

    unsigned char* data_src = (unsigned char*)src->data;
    unsigned char* data_dst = (unsigned char*)dst->data;
    int width = src->width;
    int height = src->height;
    int levels = src->levels;
    int bytesperline_src = src->bytesperline;
    int bytesperline_dst = dst->bytesperline;
    int channels_src = src->channels;
    int channels_dst = dst->channels;
    float threshold = 0.0f;
    double media = 0.0;
    double variancia = 0.0;
    double desv_padrao = 0.0;

    if ((width <= 0) || (height <= 0) || (data_src == NULL) || (data_dst == NULL)) return 0;
    if ((width != dst->width) || (height != dst->height)) return 0;
    if ((channels_dst != 1) || (channels_src != 1)) return 0;
    if ((src->levels != 255) || (dst->levels != 2)) return 0;

    int offset = kernel / 2;

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            int n = 0;
            double soma = 0;
            double soma_quad = 0;

            for (int ky = -offset; ky <= offset; ky++)
            {
                for (int kx = -offset; kx <= offset; kx++)
                {
                    int pos_vy = y + ky;
                    int pos_vx = x + kx;

                    if (pos_vy >= 0 && pos_vx >= 0 && pos_vy < height && pos_vx < width)
                    {
                        int pos_viz = pos_vy * bytesperline_src + pos_vx;
                        float valor = (float)data_src[pos_viz];

                        soma += valor;
                        soma_quad += pow(valor, 2);
                        n++;
                    }
                }
            }

            media = soma / n;

            variancia = (soma_quad / n) - pow(media, 2);

            if (variancia < 0) variancia = 0;

            desv_padrao = sqrt(variancia);

            threshold = media + k * desv_padrao;

            long int pos_src = (long int)y * bytesperline_src + x;
            long int pos_dst = (long int)y * bytesperline_dst + x;

            if (data_src[pos_src] > threshold)
            {
                data_dst[pos_dst] = 255;
            }
            else
            {
                data_dst[pos_dst] = 0;
            }
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


int vc_binary_dilate(IVC* src, IVC* dst, int kernel)
{
    if ((src->width <= 0) || (src->height <= 0) || (src->data == NULL)) return 0;
    if ((dst->width <= 0) || (dst->height <= 0) || (dst->data == NULL)) return 0;
    if ((src->width != dst->width) || (src->height != dst->height)) return 0;
    if ((src->channels != 1) || (dst->channels != 1)) return 0;
    if (kernel % 2 == 0) kernel++;

    unsigned char* datasrc = (unsigned char*)src->data;
    unsigned char* datadst = (unsigned char*)dst->data;
    unsigned char foreground = (dst->levels == 1) ? 1 : 255;
    int width = src->width;
    int height = src->height;
    int bytesperline = src->bytesperline;
    int channels = src->channels;
    int offset = (kernel - 1) / 2;
    long int pos, posk;

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            pos = y * bytesperline + x * channels;

            datadst[pos] = 0;

            for (int ky = -offset; ky <= offset; ky++)
            {
                for (int kx = -offset; kx <= offset; kx++)
                {
                    int ny = y + ky;
                    int nx = x + kx;

                    if (ny < height && ny >= 0 && nx < width && nx >= 0)
                    {
                        posk = ny * bytesperline + nx * channels;

                        if (datasrc[posk] != 0)
                        {
                            datadst[pos] = foreground;
                            break;
                        }
                    }
                }

                if (datadst[pos] == foreground)
                {
                    break;
                }
            }
        }
    }
    return 1;
}

int vc_binary_erode(IVC* src, IVC* dst, int kernel)
{
    if ((src->width <= 0) || (src->height <= 0) || (src->data == NULL)) return 0;
    if ((dst->width <= 0) || (dst->height <= 0) || (dst->data == NULL)) return 0;
    if ((src->width != dst->width) || (src->height != dst->height)) return 0;
    if ((src->channels != 1) || (dst->channels != 1)) return 0;
    if (kernel % 2 == 0) kernel++;

    unsigned char* datasrc = (unsigned char*)src->data;
    unsigned char* datadst = (unsigned char*)dst->data;
    unsigned char foreground = (dst->levels == 1) ? 1 : 255;
    int width = src->width;
    int height = src->height;
    int bytesperline = src->bytesperline;
    int channels = src->channels;
    int offset = (kernel - 1) / 2;
    long int pos, posk;

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            pos = y * bytesperline + x * channels;

            datadst[pos] = foreground;

            for (int ky = -offset; ky <= offset; ky++)
            {
                for (int kx = -offset; kx <= offset; kx++)
                {
                    int ny = y + ky;
                    int nx = x + kx;

                    if (ny < height && ny >= 0 && nx < width && nx >= 0)
                    {
                        posk = ny * bytesperline + nx * channels;

                        if (datasrc[posk] == 0)
                        {
                            datadst[pos] = 0;
                            break;
                        }
                    }
                }

                if (datadst[pos] == 0)
                {
                    break;
                }

            }
        }
    }
    return 1;
}

int vc_binary_open(IVC* src, IVC* dst, int kernel)
{
    if ((src->width <= 0) || (src->height <= 0) || (src->data == NULL)) return 0;
    if ((dst->width <= 0) || (dst->height <= 0) || (dst->data == NULL)) return 0;
    if ((src->width != dst->width) || (src->height != dst->height)) return 0;
    if ((src->channels != 1) || (dst->channels != 1)) return 0;
    if (kernel % 2 == 0) kernel++;

    IVC* temp = vc_image_new(src->width, src->height, src->channels, src->levels);
    if (temp == NULL)
    {
        return 0;
    }

    vc_binary_erode(src, temp, kernel);

    vc_binary_dilate(temp, dst, kernel);

    vc_image_free(temp);

    return 1;
}

int vc_binary_close(IVC* src, IVC* dst, int kernel)
{
    if ((src->width <= 0) || (src->height <= 0) || (src->data == NULL)) return 0;
    if ((dst->width <= 0) || (dst->height <= 0) || (dst->data == NULL)) return 0;
    if ((src->width != dst->width) || (src->height != dst->height)) return 0;
    if ((src->channels != 1) || (dst->channels != 1)) return 0;
    if (kernel % 2 == 0) kernel++;

    IVC* temp = vc_image_new(src->width, src->height, src->channels, src->levels);
    if (temp == NULL)
    {
        return 0;
    }

    vc_binary_dilate(src, temp, kernel);

    vc_binary_erode(temp, dst, kernel);

    vc_image_free(temp);

    return 1;
}

OVC* vc_binary_blob_labelling(IVC* src, IVC* dst, int* nlabels)
{
    if ((src->width <= 0) || (src->height <= 0) || (src->data == NULL)) return 0;
    if ((src->width != dst->width) || (src->height != dst->height) || (src->channels != dst->channels)) return NULL;
    if (src->channels != 1) return NULL;


    unsigned char* datasrc = (unsigned char*)src->data;
    unsigned char* datadst = (unsigned char*)dst->data;
    int width = src->width;
    int height = src->height;
    int bytesperline = src->bytesperline;
    int channels = src->channels;
    int x, y, a, b;
    long int i, size;
    long int posX, posA, posB, posC, posD;
    int labeltable[256] = { 0 };
    int labelarea[256] = { 0 };
    int label = 1;
    int num, tmplabel;
    OVC* blobs;

    memcpy(datadst, datasrc, bytesperline * height);

    for (i = 0, size = bytesperline * height; i < size; i++)
    {
        if (datadst[i] != 0) datadst[i] = 255;
    }

    for (y = 0; y < height; y++)
    {
        datadst[y * bytesperline + 0 * channels] = 0;
        datadst[y * bytesperline + (width - 1) * channels] = 0;
    }
    for (x = 0; x < width; x++)
    {
        datadst[0 * bytesperline + x * channels] = 0;
        datadst[(height - 1) * bytesperline + x * channels] = 0;
    }

    for (y = 1; y < height - 1; y++)
    {
        for (x = 1; x < width - 1; x++)
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

                    // Se A está marcado
                    if (datadst[posA] != 0) num = labeltable[datadst[posA]];
                    // Se B está marcado, e é menor que a etiqueta "num"
                    if ((datadst[posB] != 0) && (labeltable[datadst[posB]] < num)) num = labeltable[datadst[posB]];
                    // Se C está marcado, e é menor que a etiqueta "num"
                    if ((datadst[posC] != 0) && (labeltable[datadst[posC]] < num)) num = labeltable[datadst[posC]];
                    // Se D está marcado, e é menor que a etiqueta "num"
                    if ((datadst[posD] != 0) && (labeltable[datadst[posD]] < num)) num = labeltable[datadst[posD]];

                    // Atribui a etiqueta ao pixel
                    datadst[posX] = num;
                    labeltable[num] = num;

                    // Actualiza a tabela de etiquetas
                    if (datadst[posA] != 0)
                    {
                        if (labeltable[datadst[posA]] != num)
                        {
                            for (tmplabel = labeltable[datadst[posA]], a = 1; a < label; a++)
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
                            for (tmplabel = labeltable[datadst[posB]], a = 1; a < label; a++)
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
                            for (tmplabel = labeltable[datadst[posC]], a = 1; a < label; a++)
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
                            for (tmplabel = labeltable[datadst[posD]], a = 1; a < label; a++)
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
    for (y = 1; y < height - 1; y++)
    {
        for (x = 1; x < width - 1; x++)
        {
            posX = y * bytesperline + x * channels; // X

            if (datadst[posX] != 0)
            {
                datadst[posX] = labeltable[datadst[posX]];
            }
        }
    }
    // Contagem do número de blobs
   // Passo 1: Eliminar, da tabela, etiquetas repetidas
    for (a = 1; a < label - 1; a++)
    {
        for (b = a + 1; b < label; b++)
        {
            if (labeltable[a] == labeltable[b]) labeltable[b] = 0;
        }
    }

    *nlabels = 0;

    // Passo 2: Conta etiquetas e organiza a tabela de etiquetas, para que não hajam valores vazios (zero) entre etiquetas

    for (a = 1; a < label; a++)
    {
        if (labeltable[a] != 0)
        {
            labeltable[*nlabels] = labeltable[a]; // Organiza tabela de etiquetas
            (*nlabels)++; // Conta etiquetas
        }
    }

    // Se não há blobs
    if (*nlabels == 0) return NULL;

    // Cria lista de blobs (objectos) e preenche a etiqueta
    blobs = (OVC*)calloc((*nlabels), sizeof(OVC));
    if (blobs != NULL)
    {
        for (a = 0; a < (*nlabels); a++)
        {
            blobs[a].label = labeltable[a];
        }
    }
    else
    {
        return NULL;
    }

    return blobs;
}

int vc_binary_blob_labellingFake(IVC* src, IVC* dst)
{
    if ((src->width <= 0) || (src->height <= 0) || (src->data == NULL)) return 0;
    if ((src->width != dst->width) || (src->height != dst->height)) return 0;
    if ((src->channels != 1) || (dst->channels != 1)) return 0;

    unsigned char* datasrc = (unsigned char*)src->data;
    unsigned char* datadst = (unsigned char*)dst->data;
    int width = src->width;
    int height = src->height;
    int bytesperline = src->bytesperline;
    int channels = src->channels;

    unsigned char labeltable[256];
    unsigned char A = 0, B = 0, C = 0, D = 0;

    for (int i = 0; i < 256; i++)
    {
        labeltable[i] = i;
    }

    int label = 1;
    long int pos;

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            pos = y * bytesperline + x * channels;

            if (datasrc[pos] == 255)
            {
                // Se não estiver fora de bounds, a letra assume a posição da vizinhança

                A = (x > 0) ? datadst[y * src->bytesperline + (x - 1) * src->channels] : 0;
                B = (y > 0) ? datadst[(y - 1) * src->bytesperline + x * src->channels] : 0;
                C = (x > 0 && y > 0) ? datadst[(y - 1) * src->bytesperline + (x - 1) * src->channels] : 0;
                D = (x < width - 1 && y > 0) ? datadst[(y - 1) * src->bytesperline + (x + 1) * src->channels] : 0;

                if (A == 0 && B == 0 && C == 0 && D == 0)
                {
                    datadst[pos] = label;
                    label++;
                }
                else
                {
                    int min = 255;

                    if (A != 0 && A < min)
                    {
                        min = A;
                    }
                    if (B != 0 && B < min)
                    {
                        min = B;
                    }
                    if (C != 0 && C < min)
                    {
                        min = C;
                    }
                    if (D != 0 && D < min)
                    {
                        min = D;
                    }

                    datadst[pos] = min;
                }
            }
            else
            {
                datadst[pos] = 0;
            }
        }
    }
    return label - 1;
}
int vc_binary_blob_info(IVC* src, OVC* blobs, int nblobs)
{
    unsigned char* data = (unsigned char*)src->data;
    int width = src->width;
    int height = src->height;
    int bytesperline = src->bytesperline;
    int channels = src->channels;
    int x, y, i;
    long int pos;

    // Verificação de erros
    if ((src->width <= 0) || (src->height <= 0) || (src->data == NULL)) return 0;
    if (channels != 1) return 0;

    // 1. Inicializar os dados de todos os blobs a zero ANTES de percorrer a imagem
    for (i = 0; i < nblobs; i++) {
        blobs[i].area = 0;
        blobs[i].perimeter = 0;
        blobs[i].x = width - 1; // xmin
        blobs[i].y = height - 1; // ymin
        blobs[i].width = 0; // xmax temporário
        blobs[i].height = 0; // ymax temporário
        blobs[i].xc = 0; // soma do x
        blobs[i].yc = 0; // soma do y
    }

    // 2. PASSAGEM ÚNICA pela imagem (Isto mata o Lag de 1 FPS)
    for (y = 1; y < height - 1; y++) {
        for (x = 1; x < width - 1; x++) {
            pos = y * bytesperline + x * channels;
            unsigned char pixel_label = data[pos];

            // Se for um pixel branco (parte de um blob)
            if (pixel_label != 0) {
                // Procura a qual blob ele pertence
                for (i = 0; i < nblobs; i++) {
                    if (blobs[i].label == pixel_label) {
                        
                        // Área e somatório para o Centro de Massa
                        blobs[i].area++;
                        blobs[i].xc += x; 
                        blobs[i].yc += y; 

                        // Bounding Box (Extremos)
                        if (x < blobs[i].x) blobs[i].x = x;
                        if (y < blobs[i].y) blobs[i].y = y;
                        if (x > blobs[i].width) blobs[i].width = x;
                        if (y > blobs[i].height) blobs[i].height = y;

                        // Perímetro
                        if ((data[pos - 1] != pixel_label) || (data[pos + 1] != pixel_label) || 
                            (data[pos - bytesperline] != pixel_label) || (data[pos + bytesperline] != pixel_label)) {
                            blobs[i].perimeter++;
                        }
                        
                        break; // Encontrou o blob, salta para o próximo pixel
                    }
                }
            }
        }
    }

    // 3. Finalizar os cálculos matemáticos
    for (i = 0; i < nblobs; i++) {
        blobs[i].width = (blobs[i].width - blobs[i].x) + 1;
        blobs[i].height = (blobs[i].height - blobs[i].y) + 1;
        blobs[i].xc = blobs[i].xc / MAX(blobs[i].area, 1);
        blobs[i].yc = blobs[i].yc / MAX(blobs[i].area, 1);
    }

    return 1;
}
int vc_blob_segment_dark_blue(IVC *src, IVC *dst)
{
    IVC *image_hsv = NULL;
    IVC *image_seg_bin = NULL;
    IVC *image_labels = NULL;
    OVC *blobs = NULL;
    int nblobs = 0;
    int i, x, y;
    long int pos;
    unsigned char keep_label[256] = { 0 };
    
    // Parâmetros de filtro
    const int min_blob_area = 500; 
    const float min_fill_ratio = 0.1f;

    if (src == NULL || dst == NULL || src->data == NULL || dst->data == NULL) return 0;
    if (src->channels != 3 || dst->channels != 1) return 0;

    // Alocação de imagens temporárias
    image_hsv = vc_image_new(src->width, src->height, 3, 255);
    image_seg_bin = vc_image_new(src->width, src->height, 1, 255);
    image_labels = vc_image_new(src->width, src->height, 1, 255);

    if (!image_hsv || !image_seg_bin || !image_labels) goto cleanup;

    // 1. Converter para HSV
    vc_rgb_to_hsv(src, image_hsv);

    // 2. Segmentação direta para Binário (Azul Escuro)
    // Nota: Otimizei para segmentar logo para 1 canal (binário)
    for (y = 0; y < src->height; y++)
    {
        for (x = 0; x < src->width; x++)
        {
            pos = y * src->bytesperline + x * src->channels;
            int h = (int)((float)image_hsv->data[pos] / 255.0f * 360.0f);
            int s = (int)((float)image_hsv->data[pos + 1] / 255.0f * 100.0f);
            int v = (int)((float)image_hsv->data[pos + 2] / 255.0f * 100.0f);

            // Filtro para azul escuro (ajusta os valores se necessário)
            if (h >= 180 && h <= 260 && s >= 20 && v >= 10)
                image_seg_bin->data[y * image_seg_bin->bytesperline + x] = 255;
            else
                image_seg_bin->data[y * image_seg_bin->bytesperline + x] = 0;
        }
    }

    // 3. MORFOLOGIA (O segredo para a imagem da direita)
    // Remove pequenos pontos de ruído
    vc_binary_open(image_seg_bin, image_seg_bin, 3);
    // Fecha buracos dentro do objeto
    vc_binary_close(image_seg_bin, image_seg_bin, 3); 

    // 4. Etiquetagem
    blobs = vc_binary_blob_labelling(image_seg_bin, image_labels, &nblobs);
    if (!blobs || nblobs == 0) goto cleanup;

    vc_binary_blob_info(image_labels, blobs, nblobs);

    // Limpar a imagem de destino (tudo a preto)
    memset(dst->data, 0, dst->bytesperline * dst->height);

    // 5. Filtragem e Desenho
    for (i = 0; i < nblobs; i++)
    {
        float fill_ratio = (float)blobs[i].area / (blobs[i].width * blobs[i].height);

        if (blobs[i].area >= min_blob_area && fill_ratio >= min_fill_ratio)
        {
            //Pinta o interior do blob no dst (opcional, se quiseres apenas a caixa remove o loop abaixo)
            for (y = blobs[i].y; y < blobs[i].y + blobs[i].height; y++)
            {
                for (x = blobs[i].x; x < blobs[i].x + blobs[i].width; x++)
                {
                    pos = y * image_labels->bytesperline + x;
                    if (image_labels->data[pos] == blobs[i].label)
                    {
                        dst->data[pos] = 255;
                    }
                }
            }

            // Desenha a caixa e o centro (serão visíveis sobre o branco)
            vc_desenhar_marcacoes(dst, blobs[i]);        }
    }

    // Limpeza
    free(blobs);
    vc_image_free(image_hsv);
    vc_image_free(image_seg_bin);
    vc_image_free(image_labels);
    return 1;

cleanup:
    if (blobs) free(blobs);
    if (image_hsv) vc_image_free(image_hsv);
    if (image_seg_bin) vc_image_free(image_seg_bin);
    if (image_labels) vc_image_free(image_labels);
    return 0;
}

int vc_desenhar_marcacoes(IVC* dst, OVC blob)
{
    // Desenhar Caixa Delimitadora
    for (int x = blob.x; x < blob.x + blob.width; x++) {
        dst->data[blob.y * dst->bytesperline + x] = 255;
        dst->data[(blob.y + blob.height - 1) * dst->bytesperline + x] = 255;
    }
    for (int y = blob.y; y < blob.y + blob.height; y++) {
        dst->data[y * dst->bytesperline + blob.x] = 255;
        dst->data[y * dst->bytesperline + (blob.x + blob.width - 1)] = 255;
    }

    // Desenhar Centro de Gravidade 
    for (int dy = -3; dy <= 3; dy++) {
        int py = blob.yc + dy;
        if (py >= 0 && py < dst->height) dst->data[py * dst->bytesperline + blob.xc] = 0;
    }
    for (int dx = -3; dx <= 3; dx++) {
        int px = blob.xc + dx;
        if (px >= 0 && px < dst->width) dst->data[blob.yc * dst->bytesperline + px] = 0;
    }

    return 1;
}

int vc_gray_histogram_show(IVC* src, IVC* dst)
{
    if ((src == NULL) || (dst == NULL)) return 0;
    if ((src->data == NULL) || (dst->data == NULL)) return 0;
    if (src->channels != 1 || dst->channels != 1) return 0;
    if (dst->width != 256) return 0;

    unsigned char* datasrc = (unsigned char*)src->data;
    unsigned char* datadst = (unsigned char*)dst->data;
    int ni[256] = { 0 };
    float pdf[256];
    float max_pdf = 0;
    float srcImageSize = src->width * src->height;
    float dstImageSize = dst->width * dst->height;

    for (int i = 0; i < srcImageSize; i++)
    {
        ni[datasrc[i]]++;
    }

    for (int i = 0; i < 256; i++)
    {
        pdf[i] = (float)ni[i] / (float)(srcImageSize);
        if (pdf[i] > max_pdf) max_pdf = pdf[i];
    }

    memset(datadst, 0, dstImageSize);

    for (int x = 0; x < 256; x++)
    {
        int bar_height = (int)((pdf[x] / max_pdf) * (dst->height - 1));

        for (int y = (dst->height - 1); y >= (dst->height - 1 - bar_height); y--)
        {
            datadst[y * dst->width + x] = 255;
        }
    }
    return 1;
}

int vc_gray_histogram_equalization(IVC* src, IVC* dst)
{
    if ((src == NULL) || (dst == NULL)) return 0;
    if (src->width != dst->width || src->height != dst->height) return 0;
    if (src->channels != 1 || dst->channels != 1) return 0;

    unsigned char* datasrc = (unsigned char*)src->data;
    unsigned char* datadst = (unsigned char*)dst->data;
    int ni[256] = { 0 };
    float pdf[256];
    float cdf[256];
    float srcImageSize = src->width * src->height;
    float dstImageSize = dst->width * dst->height;
    int L = 256;

    for (int i = 0; i < srcImageSize; i++)
    {
        ni[datasrc[i]]++;
    }

    float sum = 0.0f;
    for (int i = 0; i < L; i++)
    {
        pdf[i] = ni[i] / (float)srcImageSize;
        sum += pdf[i];
        cdf[i] = sum;
    }

    for (int i = 0; i < srcImageSize; i++)
    {
        float val = cdf[datasrc[i]] * (float)(L - 1);

        if (val > 255) val = 255;
        if (val < 0) val = 0;

        datadst[i] = (unsigned char)(val + 0.5f);
    }
    return 1;
}

int vc_gray_edge_prewitt(IVC* src, IVC* dst, float th)
{
    if ((src == NULL) || (dst == NULL)) return 0;
    if (src->width != dst->width || src->height != dst->height) return 0;
    if (src->channels != 1 || dst->channels != 1) return 0;

    unsigned char* datasrc = (unsigned char*)src->data;
    unsigned char* datadst = (unsigned char*)dst->data;
    int width = src->width;
    int height = src->height;
    int bytesperline = src->bytesperline;
    int channels = src->channels;
    float histmax;
    int histthreshold;
    float hist[256] = { 0.0f };
    int size = width * height;
    int i;

    memset(datadst, 0, bytesperline * height);

    for (int y = 1; y < height - 1; y++)
    {
        for (int x = 1; x < width - 1; x++)
        {
            long int pX = (long int)y * bytesperline + x;

            float pA = (float)datasrc[(y - 1) * bytesperline + (x - 1)];
            float pB = (float)datasrc[(y - 1) * bytesperline + x];
            float pC = (float)datasrc[(y - 1) * bytesperline + (x + 1)];
            float pD = (float)datasrc[y * bytesperline + (x - 1)];
            float pE = (float)datasrc[y * bytesperline + (x + 1)];
            float pF = (float)datasrc[(y + 1) * bytesperline + (x - 1)];
            float pG = (float)datasrc[(y + 1) * bytesperline + x];
            float pH = (float)datasrc[(y + 1) * bytesperline + (x + 1)];

            float gx = ((pC + pE + pH) - (pA + pD + pF)) / 3.0;

            float gy = ((pF + pG + pH) - (pA + pB + pC)) / 3.0;

            datadst[pX] = (unsigned char)(sqrt((double)(gx * gx + gy * gy)) / sqrt(2.0));
        }
    }

    // Calcular o histograma com o valor das magnitudes
    for (i = 0; i < size; i++)
    {
        hist[datadst[i]]++;
    }

    // Definir o threshold.
    // O threshold é definido pelo nível de intensidade (das magnitudes)
    // quando se atinge uma determinada percentagem de pixeis, definida pelo utilizador.
    // Por exemplo, se o parâmetro 'th' tiver valor 0.8, significa the o threshold será o 
    // nível de magnitude, abaixo do qual estão pelo menos 80% dos pixeis.
    histmax = 0.0f;
    for (i = 0; i <= 255; i++)
    {
        histmax += hist[i];

        // th = Prewitt Threshold
        if (histmax >= (((float)size) * th)) break;
    }
    histthreshold = i == 0 ? 1 : i;

    // Aplicada o threshold
    for (i = 0; i < size; i++)
    {
        if (datadst[i] >= (unsigned char)histthreshold)
        {
            datadst[i] = 255;
        }
        else
        {
            datadst[i] = 0;

        }
    }

    return 1;
}

int vc_gray_edge_sobel(IVC* src, IVC* dst, float th)
{
    if ((src == NULL) || (dst == NULL)) return 0;
    if (src->width != dst->width || src->height != dst->height) return 0;
    if (src->channels != 1 || dst->channels != 1) return 0;

    unsigned char* datasrc = (unsigned char*)src->data;
    unsigned char* datadst = (unsigned char*)dst->data;
    int width = src->width;
    int height = src->height;
    int bytesperline = src->bytesperline;
    int channels = src->channels;
    float histmax;
    int histthreshold;
    float hist[256] = { 0.0f };
    int size = width * height;
    int i;
    int c = 2;

    memset(datadst, 0, bytesperline * height);

    for (int y = 1; y < height - 1; y++)
    {
        for (int x = 1; x < width - 1; x++)
        {
            long int pX = (long int)y * bytesperline + x;

            float pA = (float)datasrc[(y - 1) * bytesperline + (x - 1)];
            float pB = (float)datasrc[(y - 1) * bytesperline + x];
            float pC = (float)datasrc[(y - 1) * bytesperline + (x + 1)];
            float pD = (float)datasrc[y * bytesperline + (x - 1)];
            float pE = (float)datasrc[y * bytesperline + (x + 1)];
            float pF = (float)datasrc[(y + 1) * bytesperline + (x - 1)];
            float pG = (float)datasrc[(y + 1) * bytesperline + x];
            float pH = (float)datasrc[(y + 1) * bytesperline + (x + 1)];

            float gx = ((pC + c * pE + pH) - (pA + c * pD + pF)) / 4.0;

            float gy = ((pF + c * pG + pH) - (pA + c * pB + pC)) / 4.0;

            datadst[pX] = (unsigned char)(sqrt((double)(gx * gx + gy * gy)) / sqrt(2.0));
        }
    }

    for (i = 0; i < size; i++)
    {
        hist[datadst[i]]++;
    }

    histmax = 0.0f;
    for (i = 0; i <= 255; i++)
    {
        histmax += hist[i];

        if (histmax >= (((float)size) * th)) break;
    }
    histthreshold = i == 0 ? 1 : i;

    for (i = 0; i < size; i++)
    {
        if (datadst[i] >= (unsigned char)histthreshold)
        {
            datadst[i] = 255;
        }
        else
        {
            datadst[i] = 0;

        }
    }

    return 1;
}

int vc_gray_lowpass_mean_filter(IVC* src, IVC* dst, int kernelsize)
{
    if ((src == NULL) || (dst == NULL)) return 0;
    if ((src->data == NULL) || (dst->data == NULL)) return 0;
    if ((src->width != dst->width) || (src->height != dst->height)) return 0;
    if ((src->channels != 1) || (dst->channels != 1)) return 0;
    if (kernelsize < 1) return 0;
    if (kernelsize % 2 == 0) kernelsize++;

    unsigned char* datasrc = (unsigned char*)src->data;
    unsigned char* datadst = (unsigned char*)dst->data;
    int width = src->width;
    int height = src->height;
    int bytesperline = src->bytesperline;
    int channels = src->channels;
    int x, y, kx, ky;
    int offset;
    long int pos, posk;
    float sum;
    int count;

    offset = (kernelsize - 1) / 2;

    for (y = 0; y < height; y++)
    {
        for (x = 0; x < width; x++)
        {
            pos = y * bytesperline + x * channels;
            sum = 0.0f;
            count = 0;

            for (ky = -offset; ky <= offset; ky++)
            {
                for (kx = -offset; kx <= offset; kx++)
                {
                    int xx = x + kx;
                    int yy = y + ky;

                    if (xx >= 0 && xx < width && yy >= 0 && yy < height)
                    {
                        posk = yy * bytesperline + xx * channels;

                        sum += (float)datasrc[posk];
                        count++;
                    }
                }
            }
            datadst[pos] = (unsigned char)(sum / (float)count);
        }
    }
	return 1;
}

int vc_gray_lowpass_median_filter(IVC* src, IVC* dst, int kernelsize)
{
    if ((src == NULL) || (dst == NULL)) return 0;
    if ((src->data == NULL) || (dst->data == NULL)) return 0;
    if ((src->width != dst->width) || (src->height != dst->height)) return 0;
    if ((src->channels != 1) || (dst->channels != 1)) return 0;
    if (kernelsize < 1) return 0;
    if (kernelsize % 2 == 0) kernelsize++;

    unsigned char* datasrc = (unsigned char*)src->data;
    unsigned char* datadst = (unsigned char*)dst->data;
    int width = src->width;
    int height = src->height;
    int bytesperline = src->bytesperline;
    int channels = src->channels;
    int x, y, kx, ky;
    int offset;
    long int pos, posk;
    int count;
    unsigned char neighbors[625]; // máximo kernel 25x25
    int i, j;
    unsigned char tmp;

    offset = (kernelsize - 1) / 2;

    for (y = 0; y < height; y++)
    {
        for (x = 0; x < width; x++)
        {
            pos = y * bytesperline + x * channels;
            count = 0;

            for (ky = -offset; ky <= offset; ky++)
            {
                for (kx = -offset; kx <= offset; kx++)
                {
                    int xx = x + kx;
                    int yy = y + ky;

                    if (xx >= 0 && xx < width && yy >= 0 && yy < height)
                    {
                        posk = yy * bytesperline + xx * channels;
                        neighbors[count++] = datasrc[posk];
                    }
                }
            }
            //bubble sort
            for (i = 0; i < count - 1; i++)
            {
                for (j = i + 1; j < count; j++)
                {
                    if (neighbors[i] > neighbors[j])
                    {
                        tmp = neighbors[i];
                        neighbors[i] = neighbors[j];
                        neighbors[j] = tmp;
                    }
                }
            }
            datadst[pos] = neighbors[count / 2];
        }
    }
	return 1;
}

int vc_gray_lowpass_gaussian_filter(IVC* src, IVC* dst)
{
    if ((src == NULL) || (dst == NULL)) return 0;
    if (src->channels != 1 || dst->channels != 1) return 0;

    unsigned char* datasrc = (unsigned char*)src->data;
    unsigned char* datadst = (unsigned char*)dst->data;
    int width = src->width;
    int height = src->height;
    int bytesperline = src->bytesperline;

    int kernel[5][5] = {
        { 1,  4,  7,  4, 1 },
        { 4, 16, 26, 16, 4 },
        { 7, 26, 41, 26, 7 },
        { 4, 16, 26, 16, 4 },
        { 1,  4,  7,  4, 1 }
    };
    float kernel_sum = 273.0f; // 
    int offset = 2;

    memset(datadst, 0, bytesperline * height);  

    for (int y = offset; y < height - offset; y++)
    {
        for (int x = offset; x < width - offset; x++)
        {
            float sum = 0.0f;

            for (int ky = -offset; ky <= offset; ky++)
            {
                for (int kx = -offset; kx <= offset; kx++)
                {
                    sum += (float)datasrc[(y + ky) * bytesperline + (x + kx)] * kernel[ky + offset][kx + offset];
                }
            }
            datadst[y * bytesperline + x] = (unsigned char)(sum / kernel_sum);
        }
    }
    return 1;
}

int vc_gray_highpass_filter(IVC* src, IVC* dst)
{
    if ((src == NULL) || (dst == NULL)) return 0;
    if (src->channels != 1 || dst->channels != 1) return 0;

    unsigned char* datasrc = (unsigned char*)src->data;
    unsigned char* datadst = (unsigned char*)dst->data;
    int width = src->width;
    int height = src->height;
    int bytesperline = src->bytesperline;

    int kernel[3][3] = {
        { -1, -1, -1 },
        { -1,  8, -1 },
        { -1, -1, -1 }
    };
    int offset = 1;

    memset(datadst, 0, bytesperline * height);

    for (int y = offset; y < height - offset; y++)
    {
        for (int x = offset; x < width - offset; x++)
        {
            float sum = 0.0f;

            // Convolução
            for (int ky = -offset; ky <= offset; ky++)
            {
                for (int kx = -offset; kx <= offset; kx++)
                {
                    sum += (float)datasrc[(y + ky) * bytesperline + (x + kx)] * kernel[ky + offset][kx + offset];
                }
            }

            if (sum < 0) sum = 0;
            if (sum > 255) sum = 255;

            datadst[y * bytesperline + x] = (unsigned char)sum;
        }
    }
    return 1;
}

int vc_gray_highpass_filter_enhance(IVC* src, IVC* dst, int gain)
{
    if ((src == NULL) || (dst == NULL)) return 0;
    if (src->channels != 1 || dst->channels != 1) return 0;

    unsigned char* datasrc = (unsigned char*)src->data;
    unsigned char* datadst = (unsigned char*)dst->data;
    int width = src->width;
    int height = src->height;
    int bytesperline = src->bytesperline;

    // Máscara do slide 22 com 12 no centro 
    int kernel[3][3] = {
        { -1, -2, -1 },
        { -2, 12, -2 },
        { -1, -2, -1 }
    };
    int offset = 1;

    memset(datadst, 0, bytesperline * height);

    for (int y = offset; y < height - offset; y++)
    {
        for (int x = offset; x < width - offset; x++)
        {
            float sum = 0.0f;
            long int pos = y * bytesperline + x;

            for (int ky = -offset; ky <= offset; ky++)
            {
                for (int kx = -offset; kx <= offset; kx++)
                {
                    sum += (float)datasrc[(y + ky) * bytesperline + (x + kx)] * kernel[ky + offset][kx + offset];
                }
            }

            // Nota: sum/16.0f ajuda a normalizar o brilho se a máscara for muito forte 
            float res = (float)datasrc[pos] + (sum * (float)gain / 16.0f);

            if (res < 0) res = 0;
            if (res > 255) res = 255;

            datadst[pos] = (unsigned char)res;
        }
    }
    return 1;
}