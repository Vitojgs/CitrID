#include "segmentacao.h"

// ============================================================
// FUNCOES INTERNAS (nao expostas no header)
// ============================================================

// Converte BGR (formato OpenCV) para RGB (formato esperado pela vc_rgb_to_hsv_2)
// Faz a troca in-place: canal 0 (B) <-> canal 2 (R)
static int vc_bgr_to_rgb(IVC *image) {
    if (image == NULL || image->data == NULL || image->channels != 3) return 0;
    unsigned char *data = image->data;
    int npixels = image->width * image->height;
    unsigned char tmp;
    for (int i = 0; i < npixels; i++) {
        tmp             = data[i * 3];
        data[i * 3]     = data[i * 3 + 2];
        data[i * 3 + 2] = tmp;
    }
    return 1;
}

// ============================================================
// IMPLEMENTACAO DA API PUBLICA
// ============================================================

int vc_segmentacao_init(SegmentacaoCtx *ctx, int width, int height) {
    if (ctx == NULL) return 0;

    ctx->imageRGB   = vc_image_new(width, height, 3, 255);
    ctx->imageHSV   = vc_image_new(width, height, 3, 255);
    ctx->imageMask1 = vc_image_new(width, height, 1, 255);
    ctx->imageTmp   = vc_image_new(width, height, 1, 255);

    if (!ctx->imageRGB || !ctx->imageHSV ||
        !ctx->imageMask1 || !ctx->imageTmp) {
        vc_segmentacao_free(ctx);
        return 0;
    }
    return 1;
}

void vc_segmentacao_free(SegmentacaoCtx *ctx) {
    if (ctx == NULL) return;
    vc_image_free(ctx->imageRGB);
    vc_image_free(ctx->imageHSV);
    vc_image_free(ctx->imageMask1);
    vc_image_free(ctx->imageTmp);
    ctx->imageRGB = ctx->imageHSV = NULL;
    ctx->imageMask1 = ctx->imageTmp = NULL;
}

int vc_segmentacao(SegmentacaoCtx *ctx, cv::Mat &frame) {
    if (ctx == NULL || frame.empty()) return 0;

    int width  = frame.cols;
    int height = frame.rows;

    // --------------------------------------------------------
    // PASSO 1: BGR -> RGB
    // O OpenCV armazena pixels em BGR, a vc_rgb_to_hsv_2 espera RGB.
    // A troca e feita em C puro sem usar funcoes OpenCV extra.
    // --------------------------------------------------------
    memcpy(ctx->imageRGB->data, frame.data, width * height * 3);
    vc_bgr_to_rgb(ctx->imageRGB);

    // --------------------------------------------------------
    // PASSO 2: RGB -> HSV
    // H armazenado como [0,255] proporcional a [0,360] graus
    // S e V em [0,255]
    // --------------------------------------------------------
    vc_rgb_to_hsv_2(ctx->imageRGB, ctx->imageHSV);

    // --------------------------------------------------------
    // PASSO 3: Segmentacao por cor HSV
    //
    // Thresholds determinados por analise direta do video:
    //   H 10-44 graus: intervalo da cor laranja
    //   S >= 170: separa laranjas (S medio ~210) de macas (S medio ~184)
    //   V >= 50:  inclui zonas de sombra das laranjas (V ~57-80)
    //
    // A funcao recebe H em graus [0,360] e converte internamente:
    //   hmin_255 = (hmin_deg * 255) / 360
    // --------------------------------------------------------
    vc_hsv_segmentation(ctx->imageHSV, ctx->imageMask1, 
                        10, 44, 170, 255, 50, 255);

    // --------------------------------------------------------
    // PASSO 4: Morfologia - limpar a mascara
    //
    // OPEN  kernel=7:  remove ruido pequeno e pixels soltos
    // CLOSE kernel=21: fecha buracos internos (reflexos, pedunculo)
    //
    // NOTA: src e dst nao podem ser o mesmo buffer —
    //       por isso existe imageTmp como buffer intermedio.
    //
    // OUTPUT: ctx->imageMask1 contem a mascara binaria final
    //         pronta a ser consumida pela Pessoa 2.
    // --------------------------------------------------------
    vc_binary_open(ctx->imageMask1, ctx->imageTmp,    7);
    vc_binary_close(ctx->imageTmp,  ctx->imageMask1, 21);

    return 1;
}
