#include "segmentacao.h"
#include <iostream>
#include <string>

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

// Converte mascara binaria 3 canais (output de vc_hsv_segmentation)
// para 1 canal (necessario para vc_binary_blob_labelling)
static int vc_rgb_binary_to_gray_binary(IVC *src, IVC *dst) {
    if (src == NULL || dst == NULL || src->channels != 3 || dst->channels != 1) return 0;
    unsigned char *datasrc = src->data;
    unsigned char *datadst = dst->data;
    int npixels = src->width * src->height;
    for (int i = 0; i < npixels; i++) {
        datadst[i] = (datasrc[i * 3] == 255) ? 255 : 0;
    }
    return 1;
}

// ============================================================
// IMPLEMENTACAO DA API PUBLICA
// ============================================================

int vc_segmentacao_init(SegmentacaoCtx *ctx, int width, int height) {
    if (ctx == NULL) return 0;

    ctx->imageRGB    = vc_image_new(width, height, 3, 255);
    ctx->imageHSV    = vc_image_new(width, height, 3, 255);
    ctx->imageMask3  = vc_image_new(width, height, 3, 255);
    ctx->imageMask1  = vc_image_new(width, height, 1, 255);
    ctx->imageTmp    = vc_image_new(width, height, 1, 255);
    ctx->imageLabels = vc_image_new(width, height, 1, 255);

    if (!ctx->imageRGB || !ctx->imageHSV || !ctx->imageMask3 ||
        !ctx->imageMask1 || !ctx->imageTmp || !ctx->imageLabels) {
        vc_segmentacao_free(ctx);
        return 0;
    }
    return 1;
}

void vc_segmentacao_free(SegmentacaoCtx *ctx) {
    if (ctx == NULL) return;
    vc_image_free(ctx->imageRGB);
    vc_image_free(ctx->imageHSV);
    vc_image_free(ctx->imageMask3);
    vc_image_free(ctx->imageMask1);
    vc_image_free(ctx->imageTmp);
    vc_image_free(ctx->imageLabels);
    ctx->imageRGB = ctx->imageHSV = ctx->imageMask3 = NULL;
    ctx->imageMask1 = ctx->imageTmp = ctx->imageLabels = NULL;
}

int vc_segmentacao(SegmentacaoCtx *ctx, cv::Mat &frame,
                   OVC **blobs_out, int *nlabels_out) {

    if (ctx == NULL || blobs_out == NULL || nlabels_out == NULL) return 0;
    if (frame.empty()) return 0;

    *blobs_out   = NULL;
    *nlabels_out = 0;

    int width  = frame.cols;
    int height = frame.rows;

    // --------------------------------------------------------
    // PASSO 1: BGR -> RGB
    // O OpenCV usa BGR, a vc_rgb_to_hsv_2 espera RGB
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
    // Thresholds medidos diretamente no video:
    //   H 10-44 graus: cobre a cor laranja
    //   S >= 170: separa laranjas (S medio ~210) de macas (S medio ~184)
    //   V >= 50:  inclui zonas de sombra das laranjas (V ~57-80)
    // --------------------------------------------------------
    vc_hsv_segmentation(ctx->imageHSV, ctx->imageMask3,
                        10, 44, 170, 255, 50, 255);

    // --------------------------------------------------------
    // PASSO 4: Mascara 3 canais -> 1 canal
    // vc_binary_blob_labelling exige imagem de 1 canal
    // --------------------------------------------------------
    vc_rgb_binary_to_gray_binary(ctx->imageMask3, ctx->imageMask1);

    // --------------------------------------------------------
    // PASSO 5: Morfologia
    // OPEN  kernel=7:  remove ruido pequeno
    // CLOSE kernel=21: fecha buracos (reflexos, pedunculo)
    // NOTA: src e dst nao podem ser o mesmo buffer
    // --------------------------------------------------------
    vc_binary_open(ctx->imageMask1, ctx->imageTmp,    7);
    vc_binary_close(ctx->imageTmp,  ctx->imageMask1, 21);

    // --------------------------------------------------------
    // PASSO 6: Etiquetar blobs e calcular geometria
    // --------------------------------------------------------
    int nlabels = 0;
    OVC *blobs = vc_binary_blob_labelling(ctx->imageMask1, ctx->imageLabels, &nlabels);
    if (blobs == NULL || nlabels == 0) return 1; // sem blobs, nao e erro

    vc_binary_blob_info(ctx->imageLabels, blobs, nlabels);

    // --------------------------------------------------------
    // PASSO 7: Filtrar blobs validos
    //
    // Filtro 1: area minima (3000px) — rejeita ruido residual
    //
    // Filtro 2: ratio da bounding box (w/h normalizado para [0,1])
    //   Laranjas sao redondas -> ratio ~1.0
    //   Fragmentos de maca sao alongados -> ratio ~0.2
    //   Threshold: rejeitar se ratio < 0.65
    //
    // Filtro 3: compacidade (area / area da bounding box)
    //   Laranjas preenchem bem a bbox -> compacidade ~0.75
    //   Macas apanhadas so na borda -> compacidade ~0.25-0.46
    //   Threshold: rejeitar se compacidade < 0.55
    //
    // EXCECAO: blobs na borda do frame sao sempre aceites
    // (laranjas parcialmente fora do campo de visao)
    // --------------------------------------------------------
    int n_validos = 0;

    // Contar quantos blobs passam os filtros
    for (int i = 0; i < nlabels; i++) {
        if (blobs[i].area < 3000) continue;

        float ratio = (float)blobs[i].width / (float)blobs[i].height;
        if (ratio > 1.0f) ratio = 1.0f / ratio;

        float compacidade = (float)blobs[i].area /
                            (float)(blobs[i].width * blobs[i].height);

        int na_borda = (blobs[i].x <= 2) ||
                       (blobs[i].y <= 2) ||
                       (blobs[i].x + blobs[i].width  >= width  - 2) ||
                       (blobs[i].y + blobs[i].height >= height - 2);

        if (!na_borda && (ratio < 0.65f || compacidade < 0.55f)) continue;

        n_validos++;
    }

    if (n_validos == 0) {
        free(blobs);
        return 1;
    }

    // Alocar array com apenas os blobs validos
    OVC *blobs_validos = (OVC *)malloc(n_validos * sizeof(OVC));
    if (blobs_validos == NULL) {
        free(blobs);
        return 0;
    }

    int idx = 0;
    for (int i = 0; i < nlabels; i++) {
        if (blobs[i].area < 3000) continue;

        float ratio = (float)blobs[i].width / (float)blobs[i].height;
        if (ratio > 1.0f) ratio = 1.0f / ratio;

        float compacidade = (float)blobs[i].area /
                            (float)(blobs[i].width * blobs[i].height);

        int na_borda = (blobs[i].x <= 2) ||
                       (blobs[i].y <= 2) ||
                       (blobs[i].x + blobs[i].width  >= width  - 2) ||
                       (blobs[i].y + blobs[i].height >= height - 2);

        if (!na_borda && (ratio < 0.65f || compacidade < 0.55f)) continue;

        blobs_validos[idx++] = blobs[i];
    }

    free(blobs);

    *blobs_out   = blobs_validos;
    *nlabels_out = n_validos;
    return 1;
}

void vc_segmentacao_debug_overlay(cv::Mat &frame, OVC *blobs, int nblobs) {
    if (blobs == NULL || nblobs <= 0) return;

    for (int i = 0; i < nblobs; i++) {

        // Bounding box - cv::rectangle() [1/3]
        cv::rectangle(frame,
            cv::Point(blobs[i].x, blobs[i].y),
            cv::Point(blobs[i].x + blobs[i].width, blobs[i].y + blobs[i].height),
            cv::Scalar(0, 165, 255), 2);

        // Centroide - cv::circle() [2/3]
        cv::circle(frame,
            cv::Point(blobs[i].xc, blobs[i].yc),
            5, cv::Scalar(0, 0, 255), -1);

        // Area e perimetro (substituir pela Pessoa 3 com calibre e categoria)
        std::string str = std::string("A:").append(std::to_string(blobs[i].area))
                           .append(" P:").append(std::to_string(blobs[i].perimeter));
        cv::putText(frame, str,
            cv::Point(blobs[i].x, blobs[i].y - 5),
            cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(0, 0, 0), 2);
        cv::putText(frame, str,
            cv::Point(blobs[i].x, blobs[i].y - 5),
            cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(255, 255, 255), 1);
    }
}
