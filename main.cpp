#include <iostream>
#include <string>
#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/videoio.hpp>
#include "lib/vc.h"

#include "segmentacao.h"   // Pessoa 1
// #include "blobs.h"      // Pessoa 2 - labelling, filtros, blob_info
// #include "geometria.h"  // Pessoa 3 - calibre e categoria
// #include "tracking.h"   // Pessoa 4 - contagem acumulada e tracking

int main(void) {
    const char* videofile = "video.avi";
    cv::VideoCapture capture(videofile);

    if (!capture.isOpened()) {
        std::cerr << "Erro ao abrir o ficheiro de video!" << std::endl;
        return 1;
    }

    int width  = (int)capture.get(cv::CAP_PROP_FRAME_WIDTH);
    int height = (int)capture.get(cv::CAP_PROP_FRAME_HEIGHT);

    // --------------------------------------------------------
    // PESSOA 1 - inicializar contexto de segmentacao
    // --------------------------------------------------------
    SegmentacaoCtx segCtx;
    if (!vc_segmentacao_init(&segCtx, width, height)) {
        std::cerr << "Erro ao inicializar segmentacao!" << std::endl;
        return 1;
    }

    // --------------------------------------------------------
    // PESSOA 2 - inicializar contexto de blobs
    // --------------------------------------------------------
    // BlobsCtx blobsCtx;
    // vc_blobs_init(&blobsCtx, width, height);

    // --------------------------------------------------------
    // PESSOA 4 - inicializar tracking e contagem acumulada
    // --------------------------------------------------------
    // TrackingCtx trackCtx;
    // vc_tracking_init(&trackCtx);
    // int total_laranjas = 0;

    cv::namedWindow("VC - VIDEO",      cv::WINDOW_AUTOSIZE);
    cv::namedWindow("DEBUG - Mascara", cv::WINDOW_AUTOSIZE);

    cv::Mat frame;
    while (capture.read(frame)) {

        // --------------------------------------------------------
        // PESSOA 1 - segmentacao
        // Recebe: frame BGR
        // Produz: segCtx.imageMask1 (mascara binaria limpa)
        // --------------------------------------------------------
        vc_segmentacao(&segCtx, frame);

        // --------------------------------------------------------
        // PESSOA 2 - labelling e filtragem
        // Recebe: segCtx.imageMask1
        // Produz: OVC *blobs, int nlabels (so laranjas validas)
        // --------------------------------------------------------
        OVC *blobs   = NULL;
        int  nlabels = 0;
        // vc_blobs(&blobsCtx, segCtx.imageMask1, &blobs, &nlabels);

        // --------------------------------------------------------
        // PESSOA 4 - tracking e contagem acumulada
        // Recebe: blobs, nlabels
        // Produz: total_laranjas atualizado
        // --------------------------------------------------------
        // vc_tracking_update(&trackCtx, blobs, nlabels, &total_laranjas);

        // --------------------------------------------------------
        // PESSOA 3 - calibre, categoria e overlay por laranja
        // Recebe: frame, blobs, nlabels
        // Produz: texto desenhado sobre o frame
        // --------------------------------------------------------
        // for (int i = 0; i < nlabels; i++) {
        //     vc_overlay_laranja(frame, blobs[i]);
        // }

        // --------------------------------------------------------
        // PESSOA 4 - overlay da contagem acumulada no topo do frame
        // --------------------------------------------------------
        // std::string str = std::string("TOTAL: ")
        //                    .append(std::to_string(total_laranjas));
        // cv::putText(frame, str, cv::Point(10, 20),
        //     cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0,0,0), 2);
        // cv::putText(frame, str, cv::Point(10, 20),
        //     cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255,255,255), 1);

        // --------------------------------------------------------
        // MOSTRAR JANELAS
        // --------------------------------------------------------
        cv::imshow("VC - VIDEO", frame);
        cv::imshow("DEBUG - Mascara",
            cv::Mat(height, width, CV_8UC1, segCtx.imageMask1->data));

        if (blobs != NULL) { free(blobs); blobs = NULL; }

        if (cv::waitKey(1) == 'q') break;
    }

    // --------------------------------------------------------
    // LIMPEZA
    // --------------------------------------------------------
    vc_segmentacao_free(&segCtx);
    // vc_blobs_free(&blobsCtx);   // Pessoa 2

    capture.release();
    cv::destroyAllWindows();
    return 0;
}
