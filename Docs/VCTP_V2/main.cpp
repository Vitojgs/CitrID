// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//           INSTITUTO POLITÉCNICO DO CÁVADO E DO AVE
//                          2025/2026
//             ENGENHARIA DE SISTEMAS INFORMÁTICOS
//                    VISÃO POR COMPUTADOR
//
//         Trabalho Prático - Deteção e Classificação de Laranjas
//                         (versão melhorada)
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

#include <iostream>
#include <string>
#include <chrono>
#include <cmath>
#include <vector>
#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/videoio.hpp>

extern "C" {
#include "vc.h"
}

// ---------------------------------------------------------------
// Constantes de calibração (enunciado: 280px = 55mm)
// ---------------------------------------------------------------
#define PIXELS_POR_MM   (280.0 / 55.0)   // ~5.091 px/mm
#define MM_POR_PIXEL    (55.0 / 280.0)   // ~0.196 mm/px

// Área mínima em px para aceitar um blob como laranja.
// Diâmetro mínimo legal = 53mm → raio ≈ 135px → área ≈ π×135² ≈ 57 000 px.
// Usamos 20 000 px como margem segura (laranjas parcialmente visíveis).
#define AREA_MIN_LARANJA   20000

// Limiar de circularidade abaixo do qual a laranja é assinalada como
// "irregular" (duas laranjas sobrepostas ou objeto não circular).
// Círculo perfeito = 1.0; valor empírico para laranjas reais ≈ 0.65–0.90.
#define CIRCULARIDADE_MIN  0.55

// Distância máxima (px) entre centróides de frames consecutivas para
// considerar que é a mesma laranja no tracker.
#define TRACKER_DIST_MAX   80

// ---------------------------------------------------------------
// Estrutura do tracker de laranjas
// Regista o centróide de cada laranja única já contabilizada.
// ---------------------------------------------------------------
struct TrackerEntry {
    int xc, yc;          // centróide na última frame em que foi vista
    int frames_ausente;  // frames consecutivas sem correspondência
};

// ---------------------------------------------------------------
// Classificação segundo Regulamento CEE nº 379/71
// (diâmetro equatorial em mm)
// ---------------------------------------------------------------
const char* classificar_laranja(double diametro_mm) {
    if      (diametro_mm >= 100) return "CAL 0 (>=100mm)";
    else if (diametro_mm >=  87) return "CAL 1 (87-100mm)";
    else if (diametro_mm >=  84) return "CAL 2 (84-96mm)";
    else if (diametro_mm >=  81) return "CAL 3 (81-92mm)";
    else if (diametro_mm >=  77) return "CAL 4 (77-88mm)";
    else if (diametro_mm >=  73) return "CAL 5 (73-84mm)";
    else if (diametro_mm >=  70) return "CAL 6 (70-80mm)";
    else if (diametro_mm >=  67) return "CAL 7 (67-76mm)";
    else if (diametro_mm >=  64) return "CAL 8 (64-73mm)";
    else if (diametro_mm >=  62) return "CAL 9 (62-70mm)";
    else if (diametro_mm >=  60) return "CAL 10 (60-68mm)";
    else if (diametro_mm >=  58) return "CAL 11 (58-66mm)";
    else if (diametro_mm >=  56) return "CAL 12 (56-63mm)";
    else if (diametro_mm >=  53) return "CAL 13 (53-60mm)";
    else                         return "< MIN (<53mm)";
}

// ---------------------------------------------------------------
// Circularidade: 4π × área / perímetro²
// Valor 1.0 = círculo perfeito. Abaixo de CIRCULARIDADE_MIN
// considera-se blob irregular (sobreposição ou ruído).
// ---------------------------------------------------------------
double calcular_circularidade(int area, int perimeter) {
    if (perimeter == 0) return 0.0;
    return (4.0 * M_PI * area) / ((double)perimeter * perimeter);
}

// ---------------------------------------------------------------
// Timer (igual ao CodigoExemplo.cpp)
// ---------------------------------------------------------------
void vc_timer(void) {
    static bool running = false;
    static std::chrono::steady_clock::time_point previousTime =
        std::chrono::steady_clock::now();

    if (!running) {
        running = true;
    } else {
        auto currentTime  = std::chrono::steady_clock::now();
        auto elapsedTime  = currentTime - previousTime;
        std::chrono::duration<double> time_span =
            std::chrono::duration_cast<std::chrono::duration<double>>(elapsedTime);
        std::cout << "Tempo decorrido: " << time_span.count() << " segundos\n";
        std::cout << "Pressione qualquer tecla para continuar...\n";
        std::cin.get();
    }
}

// ---------------------------------------------------------------
// Converte frame BGR (OpenCV) → IVC RGB
// vc_rgb_to_hsv espera R, G, B nas posições 0, 1, 2.
// ---------------------------------------------------------------
void bgr_to_ivc_rgb(const cv::Mat& frame, IVC* ivc) {
    int width  = frame.cols;
    int height = frame.rows;
    const unsigned char* src = frame.data;
    unsigned char*       dst = ivc->data;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            long int pos = (long int)(y * width * 3) + (x * 3);
            dst[pos    ] = src[pos + 2]; // R
            dst[pos + 1] = src[pos + 1]; // G
            dst[pos + 2] = src[pos    ]; // B
        }
    }
}

// ---------------------------------------------------------------
// Extrai canal V (Value/Brilho) da imagem HSV para uma imagem
// grayscale de 1 canal, usada na equalização de histograma.
// ---------------------------------------------------------------
void extrair_canal_v(IVC* hsv, IVC* gray_v) {
    int width    = hsv->width;
    int height   = hsv->height;
    unsigned char* src = hsv->data;
    unsigned char* dst = gray_v->data;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            long int pos_src = (long int)(y * width * 3) + (x * 3);
            long int pos_dst = (long int)(y * width) + x;
            dst[pos_dst] = src[pos_src + 2]; // canal V (índice 2 no HSV)
        }
    }
}

// ---------------------------------------------------------------
// Repõe o canal V equalizado de volta na imagem HSV.
// ---------------------------------------------------------------
void repor_canal_v(IVC* gray_v_eq, IVC* hsv) {
    int width  = hsv->width;
    int height = hsv->height;
    unsigned char* eq  = gray_v_eq->data;
    unsigned char* dst = hsv->data;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            long int pos_hsv  = (long int)(y * width * 3) + (x * 3);
            long int pos_gray = (long int)(y * width) + x;
            dst[pos_hsv + 2] = eq[pos_gray]; // substitui canal V
        }
    }
}

// ---------------------------------------------------------------
// Tracker: tenta associar um centróide (xc, yc) a uma entrada
// existente. Devolve true se é uma laranja já contada,
// false se é nova (adiciona ao tracker).
// ---------------------------------------------------------------
bool tracker_ja_existe(std::vector<TrackerEntry>& tracker, int xc, int yc) {
    double dist_min = 1e9;
    int    idx_min  = -1;

    for (int i = 0; i < (int)tracker.size(); i++) {
        double dx   = xc - tracker[i].xc;
        double dy   = yc - tracker[i].yc;
        double dist = sqrt(dx * dx + dy * dy);
        if (dist < dist_min) {
            dist_min = dist;
            idx_min  = i;
        }
    }

    if (idx_min >= 0 && dist_min <= TRACKER_DIST_MAX) {
        // Atualiza posição e marca como vista nesta frame
        tracker[idx_min].xc = xc;
        tracker[idx_min].yc = yc;
        tracker[idx_min].frames_ausente = 0;
        return true; // já conhecida
    }

    // Nova laranja: adiciona ao tracker
    TrackerEntry nova;
    nova.xc = xc;
    nova.yc = yc;
    nova.frames_ausente = 0;
    tracker.push_back(nova);
    return false; // nova
}

// ---------------------------------------------------------------
// Remove do tracker entradas ausentes há mais de N frames.
// Evita que laranjas que saíram do ecrã "bloqueiem" novas.
// ---------------------------------------------------------------
void tracker_limpar_ausentes(std::vector<TrackerEntry>& tracker, int max_ausentes) {
    for (int i = (int)tracker.size() - 1; i >= 0; i--) {
        tracker[i].frames_ausente++;
        if (tracker[i].frames_ausente > max_ausentes) {
            tracker.erase(tracker.begin() + i);
        }
    }
}

// ---------------------------------------------------------------
// MAIN
// ---------------------------------------------------------------
int main(void) {
    // --- Vídeo ---
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

    // Contagem total de laranjas únicas (via tracker)
    int total_laranjas = 0;

    // Tracker de laranjas entre frames
    std::vector<TrackerEntry> tracker;

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

    // --- Janelas ---
    cv::namedWindow("VC - VIDEO",   cv::WINDOW_AUTOSIZE);
    cv::namedWindow("VC - BINARIO", cv::WINDOW_AUTOSIZE);

    // --- Alocar imagens IVC (reutilizadas em cada frame) ---
    IVC* image_rgb   = vc_image_new(video.width, video.height, 3, 255); // BGR→RGB
    IVC* image_hsv   = vc_image_new(video.width, video.height, 3, 255); // HSV
    IVC* image_v     = vc_image_new(video.width, video.height, 1, 255); // canal V (gray)
    IVC* image_v_eq  = vc_image_new(video.width, video.height, 1, 255); // canal V equalizado
    IVC* image_v_gau = vc_image_new(video.width, video.height, 1, 255); // canal V suavizado
    IVC* image_seg   = vc_image_new(video.width, video.height, 1, 255); // segmentação binária
    IVC* image_tmp   = vc_image_new(video.width, video.height, 1, 255); // auxiliar morfologia
    IVC* image_lbl   = vc_image_new(video.width, video.height, 1, 255); // labels dos blobs

    if (!image_rgb || !image_hsv || !image_v || !image_v_eq ||
        !image_v_gau || !image_seg || !image_tmp || !image_lbl) {
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
        // 1. BGR → RGB (IVC)
        // -------------------------------------------------------
        bgr_to_ivc_rgb(frame, image_rgb);

        // -------------------------------------------------------
        // 2. RGB → HSV
        // -------------------------------------------------------
        vc_rgb_to_hsv(image_rgb, image_hsv);

        // -------------------------------------------------------
        // 3. MELHORIA: Equalização do histograma do canal V
        //    Melhora a robustez perante variações de iluminação.
        //    Extrai V → equaliza → suaviza (Gaussian) → repõe em HSV.
        // -------------------------------------------------------
        extrair_canal_v(image_hsv, image_v);
        vc_gray_histogram_equalization(image_v, image_v_eq);
        vc_gray_lowpass_gaussian_filter(image_v_eq, image_v_gau);
        repor_canal_v(image_v_gau, image_hsv);

        // -------------------------------------------------------
        // 4. Segmentação HSV — detetar cor laranja
        //    H em graus [0,360]: laranjas ≈ 10°–40°
        //    S em [0,255]: saturação alta ≥ 80
        //    V em [0,255]: brilho médio-alto ≥ 60 (após equalização)
        // -------------------------------------------------------
        vc_hsv_segmentation(image_hsv, image_seg,
                            10, 40,   // Hmin, Hmax (graus)
                            80, 255,  // Smin, Smax
                            60, 255); // Vmin, Vmax

        // -------------------------------------------------------
        // 5. Morfologia: Open (remove ruído) + Close (preenche buracos)
        // -------------------------------------------------------
        vc_binary_open (image_seg, image_tmp, 5);
        vc_binary_close(image_tmp, image_seg, 9);

        // -------------------------------------------------------
        // 6. Blob labelling + info
        // -------------------------------------------------------
        int nblobs = 0;
        OVC* blobs = vc_binary_blob_labelling(image_seg, image_lbl, &nblobs);

        int laranjas_frame = 0;

        // Marcar todas as entradas do tracker como não vistas nesta frame
        // (incremento de ausência feito em tracker_limpar_ausentes no fim)

        if (blobs != NULL && nblobs > 0) {
            vc_binary_blob_info(image_lbl, blobs, nblobs);

            // ---------------------------------------------------
            // 7. Filtrar, analisar e anotar cada blob
            // ---------------------------------------------------
            for (int i = 0; i < nblobs; i++) {

                // --- Filtro de área mínima ---
                if (blobs[i].area < AREA_MIN_LARANJA) continue;

                // --- MELHORIA: Circularidade ---
                double circ = calcular_circularidade(blobs[i].area,
                                                     blobs[i].perimeter);
                bool irregular = (circ < CIRCULARIDADE_MIN);

                // --- Métricas de tamanho ---
                double diametro_px = (blobs[i].width + blobs[i].height) / 2.0;
                double diametro_mm = diametro_px * MM_POR_PIXEL;
                double area_mm2    = blobs[i].area * (MM_POR_PIXEL * MM_POR_PIXEL);
                double perim_mm    = blobs[i].perimeter * MM_POR_PIXEL;

                // --- Classificação por calibre ---
                const char* calibre = classificar_laranja(diametro_mm);

                // --- MELHORIA: Tracker — só conta se for nova ---
                bool ja_contada = tracker_ja_existe(tracker,
                                                    blobs[i].xc,
                                                    blobs[i].yc);
                if (!ja_contada) {
                    total_laranjas++;
                }

                laranjas_frame++;

                // --- Cor da bounding box: laranja=normal, vermelho=irregular ---
                cv::Scalar cor_box = irregular
                    ? cv::Scalar(0, 0, 220)        // vermelho: irregular
                    : cv::Scalar(0, 165, 255);      // laranja: normal

                // --- Bounding box ---
                cv::rectangle(frame,
                    cv::Point(blobs[i].x, blobs[i].y),
                    cv::Point(blobs[i].x + blobs[i].width,
                              blobs[i].y + blobs[i].height),
                    cor_box, 2);

                // --- Centro de massa ---
                cv::circle(frame,
                    cv::Point(blobs[i].xc, blobs[i].yc),
                    5, cv::Scalar(0, 0, 255), -1);

                // --- Etiqueta: calibre (linha de cima) ---
                std::string lbl_cal = irregular
                    ? std::string("IRREGULAR")
                    : std::string(calibre);

                cv::putText(frame, lbl_cal,
                    cv::Point(blobs[i].x, blobs[i].y - 30),
                    cv::FONT_HERSHEY_SIMPLEX, 0.44,
                    cv::Scalar(0, 0, 0), 2);
                cv::putText(frame, lbl_cal,
                    cv::Point(blobs[i].x, blobs[i].y - 30),
                    cv::FONT_HERSHEY_SIMPLEX, 0.44,
                    irregular ? cv::Scalar(0, 0, 255)
                              : cv::Scalar(0, 220, 255), 1);

                // --- Etiqueta: diâmetro (linha do meio) ---
                std::string lbl_dim = std::string("D:")
                    + std::to_string((int)diametro_mm) + "mm";
                cv::putText(frame, lbl_dim,
                    cv::Point(blobs[i].x, blobs[i].y - 16),
                    cv::FONT_HERSHEY_SIMPLEX, 0.42,
                    cv::Scalar(0, 0, 0), 2);
                cv::putText(frame, lbl_dim,
                    cv::Point(blobs[i].x, blobs[i].y - 16),
                    cv::FONT_HERSHEY_SIMPLEX, 0.42,
                    cv::Scalar(255, 255, 255), 1);

                // --- Etiqueta: circularidade (linha de baixo) ---
                std::string lbl_circ = std::string("C:")
                    + std::to_string((int)(circ * 100)) + "%";
                cv::putText(frame, lbl_circ,
                    cv::Point(blobs[i].x, blobs[i].y - 3),
                    cv::FONT_HERSHEY_SIMPLEX, 0.40,
                    cv::Scalar(0, 0, 0), 2);
                cv::putText(frame, lbl_circ,
                    cv::Point(blobs[i].x, blobs[i].y - 3),
                    cv::FONT_HERSHEY_SIMPLEX, 0.40,
                    cv::Scalar(180, 255, 180), 1);

                // --- Log no terminal ---
                std::cout << "[Frame " << video.nframe << "]"
                          << "  Blob=" << (i + 1)
                          << "  Area=" << blobs[i].area
                          << "px (" << (int)area_mm2 << "mm2)"
                          << "  Perim=" << blobs[i].perimeter
                          << "px (" << (int)perim_mm << "mm)"
                          << "  Diam~" << (int)diametro_mm << "mm"
                          << "  Circ=" << (int)(circ * 100) << "%"
                          << (irregular ? " [IRREGULAR]" : "")
                          << "  Calibre=" << calibre
                          << "  Centro=(" << blobs[i].xc
                          << "," << blobs[i].yc << ")"
                          << (ja_contada ? "" : " [NOVA]")
                          << "\n";
            }

            free(blobs);
        }

        // Envelhecer entradas não vistas e remover as muito antigas
        // (tolerância de 8 frames de ausência — ~0.32s a 25fps)
        tracker_limpar_ausentes(tracker, 8);

        // -------------------------------------------------------
        // 8. Painel HUD — canto superior esquerdo
        // -------------------------------------------------------
        cv::rectangle(frame,
            cv::Point(0, 0),
            cv::Point(480, 135),
            cv::Scalar(0, 0, 0), -1);

        str = std::string("RESOLUCAO: ")
            .append(std::to_string(video.width))
            .append("x").append(std::to_string(video.height));
        cv::putText(frame, str, cv::Point(10, 22),
            cv::FONT_HERSHEY_SIMPLEX, 0.62, cv::Scalar(180, 180, 180), 1);

        str = std::string("FRAME: ")
            .append(std::to_string(video.nframe))
            .append(" / ").append(std::to_string(video.ntotalframes));
        cv::putText(frame, str, cv::Point(10, 44),
            cv::FONT_HERSHEY_SIMPLEX, 0.62, cv::Scalar(180, 180, 180), 1);

        str = std::string("LARANJAS NA FRAME: ")
            .append(std::to_string(laranjas_frame));
        cv::putText(frame, str, cv::Point(10, 66),
            cv::FONT_HERSHEY_SIMPLEX, 0.62, cv::Scalar(0, 200, 255), 1);

        str = std::string("TOTAL ACUMULADO (unicas): ")
            .append(std::to_string(total_laranjas));
        cv::putText(frame, str, cv::Point(10, 88),
            cv::FONT_HERSHEY_SIMPLEX, 0.62, cv::Scalar(0, 255, 100), 1);

        str = std::string("ESCALA: 280px=55mm (~0.196mm/px)");
        cv::putText(frame, str, cv::Point(10, 110),
            cv::FONT_HERSHEY_SIMPLEX, 0.50, cv::Scalar(130, 130, 130), 1);

        str = std::string("LEGENDA: laranja=ok  vermelho=irregular");
        cv::putText(frame, str, cv::Point(10, 128),
            cv::FONT_HERSHEY_SIMPLEX, 0.44, cv::Scalar(130, 130, 130), 1);

        // -------------------------------------------------------
        // 9. Janela binária
        // -------------------------------------------------------
        cv::Mat bin_display(video.height, video.width,
                            CV_8UC1, image_seg->data);
        cv::imshow("VC - BINARIO", bin_display);

        // -------------------------------------------------------
        // 10. Janela principal
        // -------------------------------------------------------
        cv::imshow("VC - VIDEO", frame);

        key = cv::waitKey(1);
    }

    // --- Limpeza ---
    vc_timer();

    vc_image_free(image_rgb);
    vc_image_free(image_hsv);
    vc_image_free(image_v);
    vc_image_free(image_v_eq);
    vc_image_free(image_v_gau);
    vc_image_free(image_seg);
    vc_image_free(image_tmp);
    vc_image_free(image_lbl);

    cv::destroyAllWindows();
    capture.release();

    std::cout << "\n=== FIM DO PROCESSAMENTO ===\n";
    std::cout << "Total de laranjas unicas contabilizadas: "
              << total_laranjas << "\n";

    return 0;
}
