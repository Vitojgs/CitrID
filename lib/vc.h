#define VC_DEBUG

#ifndef VC_H
#define VC_H

#ifdef __cplusplus
extern "C" {
#endif

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//                   ESTRUTURA DE UMA IMAGEM
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


typedef struct {
	unsigned char *data;
	int width, height;
	int channels;			// Bin�rio/Cinzentos=1; RGB=3
	int levels;				// Bin�rio=1; Cinzentos [1,255]; RGB [1,255]
	int bytesperline;		// width * channels
} IVC;


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//                    PROT�TIPOS DE FUN��ES
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// FUN��ES: ALOCAR E LIBERTAR UMA IMAGEM
IVC *vc_image_new(int width, int height, int channels, int levels);

IVC *vc_image_free(IVC *image);

// FUN��ES: LEITURA E ESCRITA DE IMAGENS (PBM, PGM E PPM)
IVC *vc_read_image(char *filename);

int vc_write_image(const char *filename, IVC *image);

// Protótipos para extração de canais RGB
int vc_rgb_get_red_gray(IVC *src);

int vc_rgb_get_green_gray(IVC *src);

int vc_rgb_get_blue_gray(IVC *src);

int vc_rgb_to_hsv(IVC *src, IVC *dst);

int vc_hsv_segmentation(IVC *src, IVC *dst, int hmin, int hmax, int smin, int smax, int vmin, int vmax);

int vc_scale_gray_to_rgb(IVC *src, IVC *dst);

int vc_scale_gray_to_rgb2(IVC *src, IVC *dst);

int vc_pet_activity(IVC *image);

int vc_gray_to_binary(IVC *src, IVC *dst, int threshold);

int vc_gray_to_binary_range(IVC *src, IVC *dst, int thresholdmin, int thresholdmax, int mode);

int vc_gray_to_binary_midpoint(IVC *src, IVC *dst, int kernel);

int vc_gray_to_binary_bersen(IVC *src, IVC *dst, int kernel);

int vc_gray_to_binary_niblack(IVC *src, IVC *dst, int kernel, float k);

int vc_binary_erode(IVC *src, IVC *dst, int kernel);

int vc_binary_dilate(IVC *src, IVC *dst, int kernel);

int vc_binary_open(IVC *src, IVC *dst, int kernel);

int vc_binary_close(IVC *src, IVC *dst, int kernel);

//int vc_ex2brain (IVC *src, IVC *dst, int kernel);
int vc_brain_segment(IVC *src, IVC *dst);

int vc_gray_mask(IVC *src, IVC *mask, IVC *dst);


typedef struct {
	int x, y, width, height;	// Caixa Delimitadora (Bounding Box)
	int area;					// �rea
	int xc, yc;					// Centro-de-massa
	int perimeter;				// Per�metro
	int label;					// Etiqueta
} OVC;


int vc_binary_blob_info(IVC *src, OVC *blobs, int nblobs);

OVC* vc_binary_blob_labelling(IVC *src, IVC *dst, int *nlabels);

int vc_gray_histogram_show(IVC *src, IVC *dst);

int vc_gray_histogram_equalization(IVC *src, IVC *dst);

int vc_gray_edge_prewitt(IVC *src, IVC *dst, float th);

#ifdef __cplusplus
}
#endif

#endif