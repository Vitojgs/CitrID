// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//           INSTITUTO POLITÉCNICO DO CÁVADO E DO AVE
//                          2025/2026
//             ENGENHARIA DE SISTEMAS INFORMÁTICOS
//                    VISÃO POR COMPUTADOR
//
//         Trabalho Prático - Deteção e Classificação de Laranjas
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

#include <iostream>
#include <string>
#include <chrono>
#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/videoio.hpp>

extern "C" {
#include "vc.h"
}

// ---------------------------------------------------------------
// Constantes de calibração (do enunciado: 280px = 55mm)
// ---------------------------------------------------------------
#define PIXELS_POR_MM   (280.0 / 55.0)   // ~5.09 px/mm
#define MM_POR_PIXEL    (55.0 / 280.0)    // ~0.196 mm/px

// Área mínima para considerar uma laranja (evitar ruído)
// Diâmetro mínimo de uma laranja: 53mm (Regulamento CEE 379/71)
// raio_min_px = (53mm / 2) * PIXELS_POR_MM ≈ 135px
// area_min = pi * r^2 ≈ 57000 px
#define AREA_MIN_LARANJA   8000

// ---------------------------------------------------------------
// Classificação de laranjas segundo o Regulamento CEE nº 379/71
// Baseado no diâmetro equatorial (em mm)
// ---------------------------------------------------------------
const char* classificar_laranja(double diametro_mm) {
    if (diametro_mm >= 100)            return "CAL 0 (>=100mm)";
    else if (diametro_mm >= 87)        return "CAL 1 (87-100mm)";
    else if (diametro_mm >= 84)        return "CAL 2 (84-96mm)";
    else if (diametro_mm >= 81)        return "CAL 3 (81-92mm)";
    else if (diametro_mm >= 77)        return "CAL 4 (77-88mm)";
    else if (diametro_mm >= 73)        return "CAL 5 (73-84mm)";
    else if (diametro_mm >= 70)        return "CAL 6 (70-80mm)";
    else if (diametro_mm >= 67)        return "CAL 7 (67-76mm)";
    else if (diametro_mm >= 64)        return "CAL 8 (64-73mm)";
    else if (diametro_mm >= 62)        return "CAL 9 (62-70mm)";
    else if (diametro_mm >= 60)        return "CAL 10 (60-68mm)";
    else if (diametro_mm >= 58)        return "CAL 11 (58-66mm)";
    else if (diametro_mm >= 56)        return "CAL 12 (56-63mm)";
    else if (diametro_mm >= 53)        return "CAL 13 (53-60mm)";
    else                                return "< MIN (< 53mm)";
}

// ---------------------------------------------------------------
// Timer
// ---------------------------------------------------------------
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
        std::cout << "Tempo decorrido: " << nseconds << " segundos" << std::endl;
        std::cout << "Pressione qualquer tecla para continuar...\n";
        std::cin.get();
    }
}

// ---------------------------------------------------------------
// Converte frame BGR (OpenCV) para IVC com canais RGB
// (vc_rgb_to_hsv espera R,G,B na ordem pos, pos+1, pos+2)
// ---------------------------------------------------------------
void bgr_to_ivc_rgb(const cv::Mat& frame, IVC* ivc) {
    int width  = frame.cols;
    int height = frame.rows;
    unsigned char* dst = ivc->data;
    const unsigned char* src = frame.data;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            long int pos = (long int)(y * width * 3) + (x * 3);
            // OpenCV: B=0, G=1, R=2  →  IVC: R=0, G=1, B=2
            dst[pos    ] = src[pos + 2]; // R
            dst[pos + 1] = src[pos + 1]; // G
            dst[pos + 2] = src[pos    ]; // B
        }
    }
}

// ---------------------------------------------------------------
// MAIN
// ---------------------------------------------------------------
int main(void) {
    // --- Variáveis de vídeo ---
    char videofile[] = "video.avi";
    cv::VideoCapture capture;
    struct {
        int width, height;
        int ntotalframes;
        int fps;
        int nframe;
    } video;

    std::string str;
    int key = 0;

    // Contagem total acumulada de laranjas ao longo do vídeo
    int total_laranjas = 0;

    // --- Abrir vídeo ---
    capture.open(videofile);
    if (!capture.isOpened()) {
        std::cerr << "Erro ao abrir o ficheiro de vídeo!\n";
        return 1;
    }

    video.ntotalframes = (int)capture.get(cv::CAP_PROP_FRAME_COUNT);
    video.fps          = (int)capture.get(cv::CAP_PROP_FPS);
    video.width        = (int)capture.get(cv::CAP_PROP_FRAME_WIDTH);
    video.height       = (int)capture.get(cv::CAP_PROP_FRAME_HEIGHT);

    std::cout << "Video: " << video.width << "x" << video.height
              << "  FPS=" << video.fps
              << "  Frames=" << video.ntotalframes << "\n";

    // --- Criar janelas ---
    cv::namedWindow("VC - VIDEO",    cv::WINDOW_AUTOSIZE);
    cv::namedWindow("VC - BINARIO",  cv::WINDOW_AUTOSIZE);

    // --- Alocar imagens IVC (reutilizadas por frame) ---
    IVC* image_rgb  = vc_image_new(video.width, video.height, 3, 255); // BGR→RGB cópia
    IVC* image_hsv  = vc_image_new(video.width, video.height, 3, 255); // HSV
    IVC* image_seg  = vc_image_new(video.width, video.height, 1, 255); // Segmentação binária
    IVC* image_tmp  = vc_image_new(video.width, video.height, 1, 255); // Auxiliar morfologia
    IVC* image_lbl  = vc_image_new(video.width, video.height, 1, 255); // Labels dos blobs

    if (!image_rgb || !image_hsv || !image_seg || !image_tmp || !image_lbl) {
        std::cerr << "Erro ao alocar imagens IVC!\n";
        return 1;
    }

    vc_timer();

    cv::Mat frame;
    while (key != 'q') {
        capture.read(frame);
        if (frame.empty()) break;

        video.nframe = (int)capture.get(cv::CAP_PROP_POS_FRAMES);

        // -------------------------------------------------------
        // 1. BGR (OpenCV) → RGB (IVC) — vc_rgb_to_hsv espera RGB
        // -------------------------------------------------------
        bgr_to_ivc_rgb(frame, image_rgb);

        // -------------------------------------------------------
        // 2. RGB → HSV
        // -------------------------------------------------------
        vc_rgb_to_hsv(image_rgb, image_hsv);

        // -------------------------------------------------------
        // 3. Segmentação HSV — detetar a cor laranja
        //    H em graus [0,360]:  laranjas ≈ 10°–40°
        //    S em [0,255]:        saturação alta ≥ 90
        //    V em [0,255]:        valor médio-alto ≥ 80
        // -------------------------------------------------------
        vc_hsv_segmentation(image_hsv, image_seg,
                            10, 40,   // Hmin, Hmax (graus)
                            90, 255,  // Smin, Smax
                            80, 255); // Vmin, Vmax

        // -------------------------------------------------------
        // 4. Morfologia: Open (remove ruído) + Close (preenche buracos)
        // -------------------------------------------------------
        vc_binary_open (image_seg, image_tmp, 5);
        vc_binary_close(image_tmp, image_seg, 9);

        // -------------------------------------------------------
        // 5. Blob labelling
        // -------------------------------------------------------
        int nblobs = 0;
        OVC* blobs = vc_binary_blob_labelling(image_seg, image_lbl, &nblobs);

        int laranjas_frame = 0;

        if (blobs != NULL && nblobs > 0) {
            // Calcular área, perímetro, bounding box e centro de massa
            vc_binary_blob_info(image_lbl, blobs, nblobs);

            // -------------------------------------------------------
            // 6. Filtrar blobs e desenhar anotações
            // -------------------------------------------------------
            for (int i = 0; i < nblobs; i++) {
                if (blobs[i].area < AREA_MIN_LARANJA) continue;

                laranjas_frame++;

                // Diâmetro estimado: média de width e height da bounding box
                double diametro_px = (blobs[i].width + blobs[i].height) / 2.0;
                double diametro_mm = diametro_px * MM_POR_PIXEL;

                // Área real estimada (círculo equivalente) em mm²
                double area_mm2   = blobs[i].area * (MM_POR_PIXEL * MM_POR_PIXEL);
                double perim_mm   = blobs[i].perimeter * MM_POR_PIXEL;

                // Classificação por calibre
                const char* calibre = classificar_laranja(diametro_mm);

                // --- Bounding box (1 instância OpenCV extra: cv::rectangle) ---
                cv::rectangle(frame,
                    cv::Point(blobs[i].x, blobs[i].y),
                    cv::Point(blobs[i].x + blobs[i].width, blobs[i].y + blobs[i].height),
                    cv::Scalar(0, 165, 255), 2);  // laranja

                // --- Centro de massa (cv::circle é a 2ª instância extra) ---
                cv::circle(frame,
                    cv::Point(blobs[i].xc, blobs[i].yc),
                    4, cv::Scalar(0, 0, 255), -1);

                // --- Texto: calibre e diâmetro sobre cada laranja ---
                std::string info_cal = std::string(calibre);
                std::string info_dim = std::string("D:") + std::to_string((int)diametro_mm) + "mm";

                cv::putText(frame, info_cal,
                    cv::Point(blobs[i].x, blobs[i].y - 18),
                    cv::FONT_HERSHEY_SIMPLEX, 0.45,
                    cv::Scalar(0, 0, 0), 2);
                cv::putText(frame, info_cal,
                    cv::Point(blobs[i].x, blobs[i].y - 18),
                    cv::FONT_HERSHEY_SIMPLEX, 0.45,
                    cv::Scalar(0, 220, 255), 1);

                cv::putText(frame, info_dim,
                    cv::Point(blobs[i].x, blobs[i].y - 4),
                    cv::FONT_HERSHEY_SIMPLEX, 0.42,
                    cv::Scalar(0, 0, 0), 2);
                cv::putText(frame, info_dim,
                    cv::Point(blobs[i].x, blobs[i].y - 4),
                    cv::FONT_HERSHEY_SIMPLEX, 0.42,
                    cv::Scalar(255, 255, 255), 1);

                // Imprimir no terminal (debug)
                std::cout << "[Frame " << video.nframe << "] Laranja " << laranjas_frame
                          << "  Area=" << blobs[i].area << "px (" << (int)area_mm2 << "mm2)"
                          << "  Perim=" << blobs[i].perimeter << "px (" << (int)perim_mm << "mm)"
                          << "  Diam~" << (int)diametro_mm << "mm"
                          << "  Calibre: " << calibre
                          << "  Centro=(" << blobs[i].xc << "," << blobs[i].yc << ")\n";
            }

            free(blobs);
        }

        // Atualiza contagem total acumulada
        total_laranjas += laranjas_frame;

        // -------------------------------------------------------
        // 7. Painel de informação no canto superior esquerdo
        // -------------------------------------------------------
        // Fundo semi-opaco para legibilidade
        cv::rectangle(frame,
            cv::Point(0, 0),
            cv::Point(460, 115),
            cv::Scalar(0, 0, 0), -1);

        // Linha 1: Resolução
        str = std::string("RESOLUCAO: ").append(std::to_string(video.width)).append("x").append(std::to_string(video.height));
        cv::putText(frame, str, cv::Point(10, 22), cv::FONT_HERSHEY_SIMPLEX, 0.65, cv::Scalar(200, 200, 200), 1);

        // Linha 2: Frame atual / total
        str = std::string("FRAME: ").append(std::to_string(video.nframe)).append(" / ").append(std::to_string(video.ntotalframes));
        cv::putText(frame, str, cv::Point(10, 44), cv::FONT_HERSHEY_SIMPLEX, 0.65, cv::Scalar(200, 200, 200), 1);

        // Linha 3: Laranjas na frame atual
        str = std::string("LARANJAS NA FRAME: ").append(std::to_string(laranjas_frame));
        cv::putText(frame, str, cv::Point(10, 66), cv::FONT_HERSHEY_SIMPLEX, 0.65, cv::Scalar(0, 200, 255), 1);

        // Linha 4: Total acumulado
        str = std::string("TOTAL ACUMULADO: ").append(std::to_string(total_laranjas));
        cv::putText(frame, str, cv::Point(10, 88), cv::FONT_HERSHEY_SIMPLEX, 0.65, cv::Scalar(0, 255, 100), 1);

        // Linha 5: Calibração de escala
        str = std::string("ESCALA: 280px=55mm  (~").append(std::to_string((int)(MM_POR_PIXEL * 100) / 100)).append(".").append(std::to_string((int)(MM_POR_PIXEL * 1000) % 1000)).append("mm/px)");
        cv::putText(frame, str, cv::Point(10, 110), cv::FONT_HERSHEY_SIMPLEX, 0.50, cv::Scalar(150, 150, 150), 1);

        // -------------------------------------------------------
        // 8. Mostrar imagem binária numa segunda janela
        // -------------------------------------------------------
        cv::Mat bin_display(video.height, video.width, CV_8UC1, image_seg->data);
        cv::imshow("VC - BINARIO", bin_display);

        // -------------------------------------------------------
        // 9. Mostrar frame processada
        // -------------------------------------------------------
        cv::imshow("VC - VIDEO", frame);

        key = cv::waitKey(1);
    }

    // --- Limpeza ---
    vc_timer();

    vc_image_free(image_rgb);
    vc_image_free(image_hsv);
    vc_image_free(image_seg);
    vc_image_free(image_tmp);
    vc_image_free(image_lbl);

    cv::destroyAllWindows();
    capture.release();

    std::cout << "\n=== FIM DO PROCESSAMENTO ===\n";
    std::cout << "Total de laranjas contabilizadas: " << total_laranjas << "\n";

    return 0;
}
