#include <iostream>
#include <string>
#include <chrono>
#include <opencv4/opencv2/opencv.hpp>
#include <opencv4/opencv2/core.hpp>
#include <opencv4/opencv2/highgui.hpp>
#include <opencv4/opencv2/videoio.hpp>


#include <iostream>

extern "C" {
    #include "lib/vc.h" 
}

// ============================================================
// FUNCOES AUXILIARES (NAO USAM OPENCV)
// ============================================================

// Converte BGR (formato OpenCV) para RGB (formato esperado pela vc_rgb_to_hsv_2)
// Faz a troca in-place: canal 0 (B) <-> canal 2 (R)
int vc_bgr_to_rgb(IVC *image) {
    if (image == NULL || image->data == NULL || image->channels != 3) return 0;
    unsigned char *data = image->data;
    int npixels = image->width * image->height;
    unsigned char tmp;
    for (int i = 0; i < npixels; i++) {
        tmp = data[i * 3];           // guarda B
        data[i * 3]     = data[i * 3 + 2]; // B = R
        data[i * 3 + 2] = tmp;              // R = B
    }
    return 1;
}

// Converte mascara binaria 3 canais (output de vc_hsv_segmentation)
// para 1 canal (necessario para vc_binary_blob_labelling)
// Um pixel e branco (255) se qualquer canal for 255
int vc_rgb_binary_to_gray_binary(IVC *src, IVC *dst) {
    if (src == NULL || dst == NULL) return 0;
    if (src->data == NULL || dst->data == NULL) return 0;
    if (src->channels != 3 || dst->channels != 1) return 0;
    if (src->width != dst->width || src->height != dst->height) return 0;

    unsigned char *datasrc = src->data;
    unsigned char *datadst = dst->data;
    int npixels = src->width * src->height;

    for (int i = 0; i < npixels; i++) {
        // Se o pixel estiver ativo na mascara HSV (todos os canais sao 255 ou 0)
        datadst[i] = (datasrc[i * 3] == 255) ? 255 : 0;
    }
    return 1;
}

// ============================================================
// TIMER (do CodigoExemplo.cpp original)
// ============================================================
void vc_timer(void) {
    static bool running = false;
    static std::chrono::steady_clock::time_point previousTime = std::chrono::steady_clock::now();

    if (!running) {
        running = true;
    } else {
        std::chrono::steady_clock::time_point currentTime = std::chrono::steady_clock::now();
        std::chrono::steady_clock::duration elapsedTime = currentTime - previousTime;
        std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(elapsedTime);
        double nseconds = time_span.count();
        std::cout << "Tempo decorrido: " << nseconds << "segundos" << std::endl;
        std::cout << "Pressione qualquer tecla para continuar...\n";
        std::cin.get();
    }
}

// ============================================================
// MAIN
// ============================================================
int main(void) {

    // --- Configuracao do video ---
    char videofile[20] = "video.avi";
    cv::VideoCapture capture;
    struct {
        int width, height;
        int ntotalframes;
        int fps;
        int nframe;
    } video;

    std::string str;
    int key = 0;

    // --- Thresholds de segmentacao HSV ---
    // H em graus [0,360] conforme espera vc_hsv_segmentation()
    // Valores medidos diretamente no video: laranjas estao em H ~10-44 graus
    // S >= 150 para excluir a maca vermelha que tem S mais baixo (~169 medio)
    // Ajusta estes valores se a segmentacao nao estiver boa
    const int H_MIN = 10;
    const int H_MAX = 44;
    const int S_MIN = 150;
    const int S_MAX = 255;
    const int V_MIN = 80;
    const int V_MAX = 255;

    // Area minima de um blob para ser considerado laranja (em pixels)
    // Usado para rejeitar falsos positivos residuais (ex: partes da maca vermelha)
    const int AREA_MIN = 3000;

    // --- Abertura do video ---
    capture.open(videofile);
    if (!capture.isOpened()) {
        std::cerr << "Erro ao abrir o ficheiro de video!\n";
        return 1;
    }

    video.ntotalframes = (int)capture.get(cv::CAP_PROP_FRAME_COUNT);
    video.fps          = (int)capture.get(cv::CAP_PROP_FPS);
    video.width        = (int)capture.get(cv::CAP_PROP_FRAME_WIDTH);
    video.height       = (int)capture.get(cv::CAP_PROP_FRAME_HEIGHT);

    // --- Alocacao das imagens IVC (feita UMA vez, fora do loop) ---
    // Imagem RGB (copia do frame BGR convertido)
    IVC *imageRGB   = vc_image_new(video.width, video.height, 3, 255);
    // Imagem HSV (resultado da conversao)
    IVC *imageHSV   = vc_image_new(video.width, video.height, 3, 255);
    // Mascara binaria 3 canais (output de vc_hsv_segmentation)
    IVC *imageMask3 = vc_image_new(video.width, video.height, 3, 255);
    // Mascara binaria 1 canal (para morfologia e blob labelling)
    IVC *imageMask1 = vc_image_new(video.width, video.height, 1, 255);
    // Mascara temporaria para morfologia (open/close precisam de buffer)
    IVC *imageTmp   = vc_image_new(video.width, video.height, 1, 255);
    // Imagem de blobs etiquetados (output de vc_binary_blob_labelling)
    IVC *imageLabels = vc_image_new(video.width, video.height, 1, 255);

    if (!imageRGB || !imageHSV || !imageMask3 || !imageMask1 || !imageTmp || !imageLabels) {
        std::cerr << "Erro ao alocar imagens IVC!\n";
        return 1;
    }

    // --- Janelas ---
    cv::namedWindow("VC - VIDEO",    cv::WINDOW_AUTOSIZE);
    // Janelas de DEBUG - comenta estas linhas quando nao precisares
    cv::namedWindow("DEBUG - Mascara",  cv::WINDOW_AUTOSIZE);

    vc_timer();

    // ============================================================
    // LOOP PRINCIPAL
    // ============================================================
    cv::Mat frame;
    while (key != 'q') {

        capture.read(frame);
        if (frame.empty()) break;

        video.nframe = (int)capture.get(cv::CAP_PROP_POS_FRAMES);

        // --------------------------------------------------------
        // PASSO 1: Copiar frame BGR do OpenCV para IVC e converter para RGB
        // O OpenCV usa BGR, a vc_rgb_to_hsv_2 espera RGB
        // --------------------------------------------------------
        memcpy(imageRGB->data, frame.data, video.width * video.height * 3);
        vc_bgr_to_rgb(imageRGB);  // troca B<->R, agora e RGB

        // --------------------------------------------------------
        // PASSO 2: Converter RGB -> HSV
        // H armazenado como [0,255] (proporcional a 0-360 graus)
        // S e V em [0,255]
        // --------------------------------------------------------
        vc_rgb_to_hsv_2(imageRGB, imageHSV);

        // --------------------------------------------------------
        // PASSO 3: Segmentacao por cor - isola pixels laranjos
        // hmin/hmax em graus [0,360], a funcao converte internamente
        // --------------------------------------------------------
        vc_hsv_segmentation(imageHSV, imageMask3, H_MIN, H_MAX, S_MIN, S_MAX, V_MIN, V_MAX);

        // --------------------------------------------------------
        // PASSO 4: Converter mascara 3 canais -> 1 canal
        // vc_binary_blob_labelling exige imagem de 1 canal
        // --------------------------------------------------------
        vc_rgb_binary_to_gray_binary(imageMask3, imageMask1);

        // --------------------------------------------------------
        // PASSO 5: Morfologia - limpar a mascara
        //
        // OPEN (erode -> dilate): elimina ruido pequeno e pixels soltos
        //   kernel 7: suficiente para remover artefactos sem comer demasiado
        //   das laranjas
        //
        // CLOSE (dilate -> erode): fecha buracos dentro das laranjas
        //   (reflexos de luz, pedunculo verde, manchas escuras)
        //   kernel 15: as laranjas podem ter buracos grandes (reflexos)
        //
        // NOTA: open/close precisam de um buffer temporario (imageTmp)
        // --------------------------------------------------------
        vc_binary_open(imageMask1, imageTmp, 7);    // resultado em imageTmp
        vc_binary_close(imageTmp, imageMask1, 15);   // resultado em imageMask1

        // --------------------------------------------------------
        // PASSO 6: Etiquetar blobs
        // Devolve array de OVC com nlabels blobs encontrados
        // A imageLabels contem a imagem com cada blob a uma cor diferente
        // --------------------------------------------------------
        int nlabels = 0;
        OVC *blobs = vc_binary_blob_labelling(imageMask1, imageLabels, &nlabels);

        // --------------------------------------------------------
        // PASSO 7: Calcular informacao geometrica de cada blob
        // Preenche: area, perimetro, bounding box (x,y,w,h), centroide (xc,yc)
        // --------------------------------------------------------
        if (blobs != NULL && nlabels > 0) {
            vc_binary_blob_info(imageLabels, blobs, nlabels);
        }

        // --------------------------------------------------------
        // PASSO 8: Filtrar blobs validos (area >= AREA_MIN)
        // e desenhar sobre o frame com OpenCV
        //
        // FUNCOES OPENCV USADAS AQUI (das 3 adicionais permitidas):
        //   - cv::rectangle()  [1/3]
        //   - cv::circle()     [2/3]
        //   A terceira fica reservada para a Pessoa 4 ou valor acrescentado
        // --------------------------------------------------------
        int laranjas_na_frame = 0;

        if (blobs != NULL && nlabels > 0) {
            for (int i = 0; i < nlabels; i++) {

                // Rejeitar blobs demasiado pequenos (falsos positivos)
                if (blobs[i].area < AREA_MIN) continue;

                laranjas_na_frame++;

                // Bounding box - cv::rectangle() [1/3]
                cv::rectangle(
                    frame,
                    cv::Point(blobs[i].x, blobs[i].y),
                    cv::Point(blobs[i].x + blobs[i].width - 1, blobs[i].y + blobs[i].height - 1),
                    cv::Scalar(0, 165, 255),  // laranja em BGR
                    2
                );

                // Centroide - cv::circle() [2/3]
                cv::circle(
                    frame,
                    cv::Point(blobs[i].xc, blobs[i].yc),
                    5,
                    cv::Scalar(0, 0, 255),  // vermelho
                    -1  // preenchido
                );

                // Informacao de debug no frame (area e perimetro)
                // A Pessoa 3 vai substituir isto pelo calibre e categoria
                str = std::string("A:").append(std::to_string(blobs[i].area))
                       .append(" P:").append(std::to_string(blobs[i].perimeter));
                cv::putText(frame, str,
                    cv::Point(blobs[i].x, blobs[i].y - 5),
                    cv::FONT_HERSHEY_SIMPLEX, 0.4,
                    cv::Scalar(0, 0, 0), 2);
                cv::putText(frame, str,
                    cv::Point(blobs[i].x, blobs[i].y - 5),
                    cv::FONT_HERSHEY_SIMPLEX, 0.4,
                    cv::Scalar(255, 255, 255), 1);
            }

            // Liberta o array de blobs (alocado dentro de vc_binary_blob_labelling)
            free(blobs);
            blobs = NULL;
        }

        // --------------------------------------------------------
        // PASSO 9: Mostrar informacao no frame principal
        // --------------------------------------------------------
        str = std::string("FRAME: ").append(std::to_string(video.nframe))
               .append("/").append(std::to_string(video.ntotalframes));
        cv::putText(frame, str, cv::Point(10, 20), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0,0,0), 2);
        cv::putText(frame, str, cv::Point(10, 20), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255,255,255), 1);

        str = std::string("LARANJAS NA FRAME: ").append(std::to_string(laranjas_na_frame));
        cv::putText(frame, str, cv::Point(10, 45), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0,0,0), 2);
        cv::putText(frame, str, cv::Point(10, 45), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255,255,255), 1);

        // --------------------------------------------------------
        // PASSO 10: Mostrar janela de debug da mascara
        // Converte mascara 1 canal para cv::Mat para poder mostrar
        // --------------------------------------------------------
        cv::Mat maskDisplay(video.height, video.width, CV_8UC1, imageMask1->data);
        cv::imshow("DEBUG - Mascara", maskDisplay);

        // Mostrar frame principal
        cv::imshow("VC - VIDEO", frame);

        key = cv::waitKey(1);
    }

    // ============================================================
    // LIMPEZA
    // ============================================================
    vc_timer();

    vc_image_free(imageRGB);
    vc_image_free(imageHSV);
    vc_image_free(imageMask3);
    vc_image_free(imageMask1);
    vc_image_free(imageTmp);
    vc_image_free(imageLabels);

    cv::destroyAllWindows();
    capture.release();

    return 0;
}