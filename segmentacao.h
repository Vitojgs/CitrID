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
// ESTRUTURA DE CONTEXTO DA SEGMENTACAO
//
// Agrupa todos os buffers IVC necessarios para o processamento.
// Alocados UMA vez fora do loop e reutilizados em cada frame.
// ============================================================
typedef struct {
    IVC *imageRGB;    // frame convertida de BGR para RGB
    IVC *imageHSV;    // imagem no espaco HSV
    IVC *imageMask3;  // mascara binaria 3 canais (output de vc_hsv_segmentation)
    IVC *imageMask1;  // mascara binaria 1 canal (para morfologia e labelling)
    IVC *imageTmp;    // buffer temporario para morfologia (open/close)
    IVC *imageLabels; // imagem com blobs etiquetados
} SegmentacaoCtx;

// ============================================================
// PROTOTIPOS
// ============================================================

// Aloca os buffers IVC do contexto de segmentacao.
// Deve ser chamada UMA vez antes do loop principal.
// Retorna 1 em caso de sucesso, 0 em caso de erro.
int vc_segmentacao_init(SegmentacaoCtx *ctx, int width, int height);

// Liberta os buffers IVC do contexto.
// Deve ser chamada uma vez no fim do programa.
void vc_segmentacao_free(SegmentacaoCtx *ctx);

// Processa um frame e devolve os blobs de laranjas validos.
//
// Parametros:
//   ctx         - contexto previamente inicializado
//   frame       - frame BGR do OpenCV (lida com capture.read())
//   blobs_out   - array de blobs validos (alocado internamente, libertar com free())
//   nlabels_out - numero de blobs validos devolvidos
//
// Retorna 1 em caso de sucesso, 0 em caso de erro.
// O chamador e responsavel por libertar blobs_out com free().
int vc_segmentacao(SegmentacaoCtx *ctx, cv::Mat &frame,
                   OVC **blobs_out, int *nlabels_out);

// Desenha o overlay de debug no frame (bounding box + centroide + area/perimetro).
// Chamada opconal - apenas para debug durante o desenvolvimento.
// A Pessoa 3 substitui esta funcao pelo overlay com calibre e categoria.
void vc_segmentacao_debug_overlay(cv::Mat &frame, OVC *blobs, int nblobs);

#endif
