#ifndef SEGMENTACAO_H
#define SEGMENTACAO_H

#include <opencv4/opencv2/opencv.hpp>

#ifdef __cplusplus
extern "C" {
#endif
#include "lib/vc.h"
#ifdef __cplusplus
}
#endif

// ============================================================
// ESTRUTURA DE CONTEXTO DA SEGMENTACAO (Pessoa 1)
//
// Agrupa todos os buffers IVC necessarios para o processamento.
// Alocados UMA vez fora do loop e reutilizados em cada frame.
// ============================================================
typedef struct {
    IVC *imageRGB;    // frame convertida de BGR para RGB
    IVC *imageHSV;    // imagem no espaco HSV
    IVC *imageMask3;  // mascara binaria 3 canais (output de vc_hsv_segmentation)
    IVC *imageMask1;  // mascara binaria 1 canal apos morfologia (OUTPUT FINAL)
    IVC *imageTmp;    // buffer temporario para morfologia
} SegmentacaoCtx;

// Aloca os buffers IVC. Chamar UMA vez antes do loop.
// Retorna 1 em sucesso, 0 em erro.
int vc_segmentacao_init(SegmentacaoCtx *ctx, int width, int height);

// Liberta os buffers IVC. Chamar no fim do programa.
void vc_segmentacao_free(SegmentacaoCtx *ctx);

// Processa um frame e produz a mascara binaria limpa em ctx->imageMask1.
// A mascara e o output que a Pessoa 2 vai consumir para fazer o labelling.
// Retorna 1 em sucesso, 0 em erro.
int vc_segmentacao(SegmentacaoCtx *ctx, cv::Mat &frame);

#endif
