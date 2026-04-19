#include <iostream>
#include <string>
#include <chrono>
#include <opencv4/opencv2/opencv.hpp>
#include <opencv4/opencv2/core.hpp>
#include <opencv4/opencv2/highgui.hpp>
#include <opencv4/opencv2/videoio.hpp>

extern "C" {
    #include "lib/vc.h"
}

// ============================================================
// FUNCOES AUXILIARES
// ============================================================

// Converte BGR (formato OpenCV) para RGB (formato esperado pela vc_rgb_to_hsv_2)
// Faz a troca in-place: canal 0 (B) <-> canal 2 (R)
int vc_bgr_to_rgb(IVC *image) {
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
int vc_rgb_binary_to_gray_binary(IVC *src, IVC *dst) {
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
// MAIN
// ============================================================
int main(void) {
    const char* videofile = "video.avi";
    cv::VideoCapture capture(videofile);

    if (!capture.isOpened()) {
        std::cerr << "Erro ao abrir o ficheiro de video!" << std::endl;
        return 1;
    }

    int width  = (int)capture.get(cv::CAP_PROP_FRAME_WIDTH);
    int height = (int)capture.get(cv::CAP_PROP_FRAME_HEIGHT);

    // --- Alocacao de buffers IVC (feita UMA vez, fora do loop) ---
    IVC *imageRGB    = vc_image_new(width, height, 3, 255);
    IVC *imageHSV    = vc_image_new(width, height, 3, 255);
    IVC *imageMask3  = vc_image_new(width, height, 3, 255);
    IVC *imageMask1  = vc_image_new(width, height, 1, 255);
    IVC *imageTmp    = vc_image_new(width, height, 1, 255);
    IVC *imageLabels = vc_image_new(width, height, 1, 255);

    if (!imageRGB || !imageHSV || !imageMask3 || !imageMask1 || !imageTmp || !imageLabels) {
        std::cerr << "Erro ao alocar memoria para as imagens IVC!" << std::endl;
        return 1;
    }

    cv::namedWindow("VC - VIDEO",      cv::WINDOW_AUTOSIZE);
    cv::namedWindow("DEBUG - Mascara", cv::WINDOW_AUTOSIZE);

    cv::Mat frame;
    while (capture.read(frame)) {

        // --------------------------------------------------------
        // PASSO 1: Transferencia BGR -> IVC RGB
        // --------------------------------------------------------
        memcpy(imageRGB->data, frame.data, width * height * 3);
        vc_bgr_to_rgb(imageRGB);

        // --------------------------------------------------------
        // PASSO 2: Conversao RGB -> HSV e segmentacao por cor
        //
        // Thresholds em graus [0,360] para vc_hsv_segmentation():
        //   H 10-44: cobre a cor laranja
        //   S >= 170: exclui a maca vermelha (S medio ~184) sem cortar
        //             as laranjas (S medio ~210). Era 150, subiu para 170.
        //   V >= 50:  inclui zonas de sombra das laranjas (V ~57-80)
        // --------------------------------------------------------
        vc_rgb_to_hsv_2(imageRGB, imageHSV);
        vc_hsv_segmentation(imageHSV, imageMask3, 10, 44, 170, 255, 50, 255);

        // --------------------------------------------------------
        // PASSO 3: Converter mascara 3 canais -> 1 canal
        // vc_binary_blob_labelling exige imagem de 1 canal
        // --------------------------------------------------------
        vc_rgb_binary_to_gray_binary(imageMask3, imageMask1);

        // --------------------------------------------------------
        // PASSO 4: Morfologia - limpar a mascara
        // OPEN  kernel=7:  remove ruido pequeno
        // CLOSE kernel=21: fecha buracos (reflexos, pedunculo)
        //   Era 15, subiu para 21 para fechar melhor os reflexos
        // NOTA: src e dst nao podem ser o mesmo buffer
        // --------------------------------------------------------
        vc_binary_open(imageMask1, imageTmp,    7);
        vc_binary_close(imageTmp,  imageMask1, 21);

        // --------------------------------------------------------
        // PASSO 5: Etiquetar blobs e calcular geometria
        // --------------------------------------------------------
        int nlabels = 0;
        OVC *blobs = vc_binary_blob_labelling(imageMask1, imageLabels, &nlabels);

        if (blobs != NULL && nlabels > 0) {
            vc_binary_blob_info(imageLabels, blobs, nlabels);

            for (int i = 0; i < nlabels; i++) {

                // --- Filtro 1: area minima ---
                if (blobs[i].area < 3000) continue;

                // --- Filtro 2: ratio e compacidade ---
                //
                // RATIO (w/h normalizado para [0,1]):
                //   Laranjas sao redondas -> ratio ~1.0
                //   Fragmentos da maca vermelha sao alongados -> ratio ~0.2
                //
                // COMPACIDADE (area / area da bounding box):
                //   Laranjas preenchem bem a bbox -> compacidade ~0.75
                //   Macas apanhadas so na borda/fragmentos -> compacidade ~0.25-0.46
                //
                // EXCECAO: blobs na borda do frame sao sempre aceites
                // (laranjas parcialmente fora do campo de visao)
                float ratio = (float)blobs[i].width / (float)blobs[i].height;
                if (ratio > 1.0f) ratio = 1.0f / ratio;

                float compacidade = (float)blobs[i].area /
                                    (float)(blobs[i].width * blobs[i].height);

                int na_borda = (blobs[i].x <= 2) ||
                               (blobs[i].y <= 2) ||
                               (blobs[i].x + blobs[i].width  >= width  - 2) ||
                               (blobs[i].y + blobs[i].height >= height - 2);

                if (!na_borda && (ratio < 0.65f || compacidade < 0.55f)) continue;

                // --- Blob valido: desenhar overlay ---

                // Bounding box - cv::rectangle() [1/3]
                cv::rectangle(frame,
                    cv::Point(blobs[i].x, blobs[i].y),
                    cv::Point(blobs[i].x + blobs[i].width, blobs[i].y + blobs[i].height),
                    cv::Scalar(0, 165, 255), 2);

                // Centroide - cv::circle() [2/3]
                cv::circle(frame,
                    cv::Point(blobs[i].xc, blobs[i].yc),
                    5, cv::Scalar(0, 0, 255), -1);

                // Informacao de debug (a Pessoa 3 substitui pelo calibre e categoria)
                std::string str = std::string("A:").append(std::to_string(blobs[i].area))
                                   .append(" P:").append(std::to_string(blobs[i].perimeter));
                cv::putText(frame, str,
                    cv::Point(blobs[i].x, blobs[i].y - 5),
                    cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(0, 0, 0), 2);
                cv::putText(frame, str,
                    cv::Point(blobs[i].x, blobs[i].y - 5),
                    cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(255, 255, 255), 1);
            }

            free(blobs);
            blobs = NULL;
        }

        // --------------------------------------------------------
        // PASSO 6: Mostrar janelas
        // --------------------------------------------------------
        cv::imshow("VC - VIDEO",      frame);
        cv::imshow("DEBUG - Mascara", cv::Mat(height, width, CV_8UC1, imageMask1->data));

        if (cv::waitKey(1) == 'q') break;
    }

    // --- Limpeza ---
    vc_image_free(imageRGB);
    vc_image_free(imageHSV);
    vc_image_free(imageMask3);
    vc_image_free(imageMask1);
    vc_image_free(imageTmp);
    vc_image_free(imageLabels);

    capture.release();
    cv::destroyAllWindows();
    return 0;
}