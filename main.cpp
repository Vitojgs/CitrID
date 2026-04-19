#include <iostream>
#include <string>
#include <opencv4/opencv2/opencv.hpp>
#include <opencv4/opencv2/core.hpp>
#include <opencv4/opencv2/highgui.hpp>
#include <opencv4/opencv2/videoio.hpp>

#include "segmentacao.h"
// #include "geometria.h"   // Pessoa 3 - calibre e categoria
// #include "tracking.h"    // Pessoa 4 - contagem e tracking

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
        // PESSOA 1 - segmentacao: devolve blobs validos (so laranjas)
        // --------------------------------------------------------
        OVC *blobs     = NULL;
        int  nlabels   = 0;
        vc_segmentacao(&segCtx, frame, &blobs, &nlabels);

        // --------------------------------------------------------
        // PESSOA 4 - tracking e contagem acumulada
        // --------------------------------------------------------
        // vc_tracking_update(&trackCtx, blobs, nlabels, &total_laranjas);

        // --------------------------------------------------------
        // PESSOA 3 - calibre e categoria de cada laranja
        // --------------------------------------------------------
        // for (int i = 0; i < nlabels; i++) {
        //     int calibre       = vc_calcular_calibre(blobs[i]);
        //     const char *categ = vc_calcular_categoria(blobs[i]);
        //     vc_overlay_laranja(frame, blobs[i], calibre, categ);
        // }

        // --------------------------------------------------------
        // DEBUG - overlay provisorio (remover quando Pessoa 3 integrar)
        // --------------------------------------------------------
        vc_segmentacao_debug_overlay(frame, blobs, nlabels);

        // --------------------------------------------------------
        // PESSOA 4 - mostrar contagem acumulada no frame
        // --------------------------------------------------------
        // std::string str = std::string("TOTAL: ").append(std::to_string(total_laranjas));
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

        // Libertar blobs apos uso em cada frame
        if (blobs != NULL) { free(blobs); blobs = NULL; }

        if (cv::waitKey(1) == 'q') break;
    }

    // --------------------------------------------------------
    // LIMPEZA
    // --------------------------------------------------------
    vc_segmentacao_free(&segCtx);
    capture.release();
    cv::destroyAllWindows();
    return 0;
}
