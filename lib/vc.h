//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//           INSTITUTO POLITÉCNICO DO CÁVADO E DO AVE
//                          2025/2026
//             ENGENHARIA DE SISTEMAS INFORMÁTICOS
//                    VISÃO POR COMPUTADOR
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

#ifndef VC_H  
#define VC_H  

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <malloc.h>
#include <math.h>
#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

#define VC_DEBUG

// ---------------------------------------------------------------
// Calibracao (enunciado: 280px = 55mm)
// ---------------------------------------------------------------
#define PIXELS_POR_MM   (280.0 / 55.0)
#define MM_POR_PIXEL    (55.0  / 280.0)

// Area minima: laranjas parcialmente visiveis podem ter area baixa
// Definido empiricamente para aceitar laranjas a entrar no frame.
#define AREA_MIN_LARANJA  15000

// Circularidade: 4*pi*area/perimetro^2.  Circulo perfeito = 1.0.
// Blobs abaixo de REJEITAR sao ignorados (maca circ~0.20-0.43).
// Blobs entre REJEITAR e MIN sao marcados IRREGULAR.
// Laranjas integras: circ~0.70-0.80.
#define CIRC_REJEITAR  0.50
#define CIRC_MIN       0.65

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//                   ESTRUTURA DE UMA IMAGEM
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

typedef struct IVC{
	unsigned char* data;
	int width, height;
	int channels;			// Binário/Cinzentos=1; RGB=3
	int levels;				// Binário=1; Cinzentos [1,255]; RGB [1,255]
	int bytesperline;		// width * channels
} IVC;

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//                    ESTRUTURA DE UM BLOB
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

typedef struct OVC {
	int x, y, width, height;	// Caixa Delimitadora (Bounding Box)
	int area;					// Área
	int xc, yc;					// Centro-de-massa
	int perimeter;				// Perímetro
	int label;					// Etiqueta
} OVC;

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//                    PROTÓTIPOS DE FUNÇÕES
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// ---------------------------------------------------------------
// Circularidade: 4*pi*area / perimetro^2
// ---------------------------------------------------------------
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

double circularidade(int area, int perim);

// ---------------------------------------------------------------
// Classificacao segundo Regulamento CEE n. 379/71
// Usado para encontrar os calibres das laranjas
// ---------------------------------------------------------------

const char* classificar(double dmm);

// FUNÇÕES: ALOCAR E LIBERTAR UMA IMAGEM
IVC* vc_image_new(int width, int height, int channels, int levels);
IVC* vc_image_free(IVC* image);

// FUNÇÕES: LEITURA E ESCRITA DE IMAGENS (PBM, PGM E PPM)
IVC* vc_read_image(char* filename);
int vc_write_image(char* filename, IVC* image);

//VC4
int vc_gray_negative(IVC* srcdst);
int vc_rgb_negative(IVC* srcdst);
int vc_rgb_get_red_gray(IVC* srcdst);
int vc_rgb_get_green_gray(IVC* srcdst);
int vc_rgb_get_blue_gray(IVC* srcdst);
int vc_rgb_to_gray(IVC* src, IVC* dst);
int vc_rgb_to_hsv(IVC* src, IVC* dst);
int vc_hsv_segmentation(IVC* src, IVC* dst, int hmin, int hmax, int smin, int smax, int vmin, int vmax);
int vc_scale_gray_to_rgb(IVC* src, IVC* dst);

//VC_EX
int vc_count_white_pixels(IVC* dst);

//VC5
int vc_gray_to_binary(IVC* src, IVC* dst, int threshold);
int vc_gray_to_binary_global_mean(IVC* src, IVC* dst);
int vc_gray_to_binary_midpoint(IVC* src, IVC* dst, int kernel);
int vc_gray_to_binary_bernsen(IVC* src, IVC* dst, int kernel);
int vc_gray_to_binary_niblack(IVC* src, IVC* dst, int kernel, float k);
int vc_gray_to_binary_range(IVC *src, IVC *dst, int thresholdmin, int thresholdmax, int mode);

//VC6
int vc_binary_dilate(IVC* src, IVC* dst, int kernel);
int vc_binary_erode(IVC* src, IVC* dst, int kernel);
int vc_binary_open(IVC* src, IVC* dst, int kernel);
int vc_binary_close(IVC* src, IVC* dst, int kernel);

//VC7
int vc_binary_blob_labellingFake(IVC* src, IVC* dst);
OVC* vc_binary_blob_labelling(IVC* src, IVC* dst, int* nlabels);
int vc_binary_blob_info(IVC* src, OVC* blobs, int nblobs);
int vc_blob_segment_dark_blue(IVC *src, IVC *dst);

//VC7_EX3
int vc_desenhar_marcacoes(IVC* dst, OVC blob);

//VC8
int vc_gray_histogram_show(IVC* src, IVC* dst);
int vc_gray_histogram_equalization(IVC* src, IVC* dst);

//VC9
int vc_gray_edge_prewitt(IVC* src, IVC* dst, float th);
int vc_gray_edge_sobel(IVC* src, IVC* dst, float th);

//VC10
int vc_gray_lowpass_mean_filter(IVC* src, IVC* dst, int kernelsize);
int vc_gray_lowpass_median_filter(IVC* src, IVC* dst, int kernelsize);
int vc_gray_lowpass_gaussian_filter(IVC* src, IVC* dst);
int vc_gray_highpass_filter(IVC* src, IVC* dst);
int vc_gray_highpass_filter_enhance(IVC* src, IVC* dst, int gain);

//VCTP
int vc_bgr_to_hsv(IVC* src, IVC* dst);

#endif