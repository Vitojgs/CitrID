// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//           INSTITUTO POLITÉCNICO DO CÁVADO E DO AVE
//                          2025/2026
//             ENGENHARIA DE SISTEMAS INFORMÁTICOS
//                    VISÃO POR COMPUTADOR
//
//         Trabalho Prático - Deteção e Classificação de Laranjas
//                         (versão final)
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
// Calibração (enunciado: 280px = 55mm)
// ---------------------------------------------------------------
#define PIXELS_POR_MM   (280.0 / 55.0)
#define MM_POR_PIXEL    (55.0  / 280.0)

// Área mínima em px para aceitar um blob como laranja.
// Diâmetro mínimo legal 53mm -> raio~135px -> área~57000px.
// Valor conservador para laranjas parcialmente visíveis.
#define AREA_MIN_LARANJA   15000

// Circularidade mínima: 4*pi*area/perimetro^2.
// Círculo perfeito = 1.0. Abaixo disto o blob é marcado IRREGULAR.
#define CIRCULARIDADE_MIN  0.55

// Distância máxima (px) entre centróides de frames consecutivas
// para associar um blob à mesma laranja no tracker.
#define TRACKER_DIST_MAX   120

// Número máximo de posições no histórico do rastro de trajetória.
// 20 posições a 25fps = ~0.8s de rastro visível.
#define RASTRO_MAX_POS     20

// Linha de contagem: Y a meio do frame (720/2 = 360).
// As laranjas descem de Y=0 para Y=720, logo são contadas quando
// o centróide cruza esta linha de cima para baixo (yc_ant < LINHA <= yc_novo).
#define LINHA_CONTAGEM     360

// ---------------------------------------------------------------
// Estruturas auxiliares
// ---------------------------------------------------------------
struct Ponto { int x, y; };

struct TrackerEntry {
    int xc, yc;                 // centróide na última frame vista
    int frames_ausente;         // frames consecutivas sem correspondência
    int id;                     // ID único da laranja
    bool ja_contada;            // true após cruzar a linha de contagem
    std::vector<Ponto> rastro;  // histórico dos centróides (para cv::line)
};

// ---------------------------------------------------------------
// Paleta de cores para rastros e bounding boxes (BGR)
// ---------------------------------------------------------------
static const cv::Scalar CORES_RASTRO[] = {
    cv::Scalar( 50, 220, 255),  // amarelo-torrado
    cv::Scalar(255, 100,  50),  // azul-claro
    cv::Scalar( 50, 255, 150),  // verde-limão
    cv::Scalar(200,  50, 255),  // magenta
    cv::Scalar(255, 200,  50),  // ciano
    cv::Scalar( 50,  50, 255),  // vermelho
    cv::Scalar(150, 255,  50),  // verde-amarelado
    cv::Scalar(255,  50, 200),  // rosa
};
#define N_CORES_RASTRO 8

// ---------------------------------------------------------------
// Classificação — Regulamento CEE nº 379/71
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
// Circularidade: 4*pi*area / perimetro^2
// ---------------------------------------------------------------
double calcular_circularidade(int area, int perimeter) {
    if (perimeter == 0) return 0.0;
    return (4.0 * M_PI * (double)area) / ((double)perimeter * (double)perimeter);
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
        auto currentTime = std::chrono::steady_clock::now();
        std::chrono::duration<double> ts =
            std::chrono::duration_cast<std::chrono::duration<double>>(
                currentTime - previousTime);
        std::cout << "Tempo decorrido: " << ts.count() << " segundos\n";
        std::cout << "Pressione qualquer tecla para continuar...\n";
        std::cin.get();
    }
}

// ---------------------------------------------------------------
// BGR (OpenCV) -> RGB (IVC)
// vc_rgb_to_hsv espera R, G, B nas posicoes 0, 1, 2.
// ---------------------------------------------------------------
void bgr_to_ivc_rgb(const cv::Mat& frame, IVC* ivc) {
    int width  = frame.cols;
    int height = frame.rows;
    const unsigned char* src = frame.data;
    unsigned char*       dst = ivc->data;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            long int pos    = (long int)(y * width * 3) + (x * 3);
            dst[pos    ]    = src[pos + 2]; // R
            dst[pos + 1]    = src[pos + 1]; // G
            dst[pos + 2]    = src[pos    ]; // B
        }
    }
}

// ---------------------------------------------------------------
// Extrai o canal V (índice 2) do IVC HSV para um IVC grayscale.
// ---------------------------------------------------------------
void extrair_canal_v(IVC* hsv, IVC* gray_v) {
    int width = hsv->width, height = hsv->height;
    unsigned char* src = hsv->data;
    unsigned char* dst = gray_v->data;
    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++) {
            dst[(long int)(y * width) + x] =
                src[(long int)(y * width * 3) + (x * 3) + 2];
        }
}

// ---------------------------------------------------------------
// Repõe o canal V processado de volta no IVC HSV.
// ---------------------------------------------------------------
void repor_canal_v(IVC* gray_v, IVC* hsv) {
    int width = hsv->width, height = hsv->height;
    unsigned char* src = gray_v->data;
    unsigned char* dst = hsv->data;
    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++) {
            dst[(long int)(y * width * 3) + (x * 3) + 2] =
                src[(long int)(y * width) + x];
        }
}

// ---------------------------------------------------------------
// Tracker: associa (xc, yc) a uma entrada existente ou cria nova.
// Devolve o indice da entrada (nova ou existente).
// ---------------------------------------------------------------
int tracker_associar(std::vector<TrackerEntry>& tracker,
                     int xc, int yc, int& proximo_id) {
    double dist_min = 1e9;
    int    idx_min  = -1;

    for (int i = 0; i < (int)tracker.size(); i++) {
        double dx   = xc - tracker[i].xc;
        double dy   = yc - tracker[i].yc;
        double dist = sqrt(dx * dx + dy * dy);
        if (dist < dist_min) { dist_min = dist; idx_min = i; }
    }

    if (idx_min >= 0 && dist_min <= TRACKER_DIST_MAX) {
        // Laranja conhecida — actualiza posição e rastro
        tracker[idx_min].xc             = xc;
        tracker[idx_min].yc             = yc;
        tracker[idx_min].frames_ausente = 0;
        Ponto p = { xc, yc };
        tracker[idx_min].rastro.push_back(p);
        if ((int)tracker[idx_min].rastro.size() > RASTRO_MAX_POS)
            tracker[idx_min].rastro.erase(tracker[idx_min].rastro.begin());
        return idx_min;
    }

    // Nova laranja
    TrackerEntry nova;
    nova.xc             = xc;
    nova.yc             = yc;
    nova.frames_ausente = 0;
    nova.id             = proximo_id++;
    nova.ja_contada     = false;
    Ponto p = { xc, yc };
    nova.rastro.push_back(p);
    tracker.push_back(nova);
    return (int)tracker.size() - 1;
}

// ---------------------------------------------------------------
// Envelhece entradas e remove as ausentes há > max_ausentes frames.
// ---------------------------------------------------------------
void tracker_limpar_ausentes(std::vector<TrackerEntry>& tracker,
                             int max_ausentes) {
    for (int i = (int)tracker.size() - 1; i >= 0; i--) {
        tracker[i].frames_ausente++;
        if (tracker[i].frames_ausente > max_ausentes)
            tracker.erase(tracker.begin() + i);
    }
}

// ---------------------------------------------------------------
// Desenha o rastro de trajetória com cv::line().
// Segmentos mais antigos -> mais escuros e finos (desvanecimento).
// ---------------------------------------------------------------
void desenhar_rastro(cv::Mat& frame, const TrackerEntry& entrada) {
    int n = (int)entrada.rastro.size();
    if (n < 2) return;
    cv::Scalar cor_base = CORES_RASTRO[entrada.id % N_CORES_RASTRO];
    for (int i = 1; i < n; i++) {
        double fator     = (double)i / (double)(n - 1); // 0=antigo, 1=recente
        int    espessura = (int)(1.0 + fator * 2.0);    // 1px a 3px
        cv::Scalar cor(
            cor_base[0] * fator,
            cor_base[1] * fator,
            cor_base[2] * fator
        );
        cv::line(frame,
            cv::Point(entrada.rastro[i - 1].x, entrada.rastro[i - 1].y),
            cv::Point(entrada.rastro[i    ].x, entrada.rastro[i    ].y),
            cor, espessura);
    }
}

// ---------------------------------------------------------------
// MAIN
// ---------------------------------------------------------------
int main(void) {
    char videofile[] = "video.avi";
    cv::VideoCapture capture;
    struct { int width, height, ntotalframes, fps, nframe; } video;

    std::string str;
    int key = 0;

    // Contador de laranjas que cruzaram a linha de contagem
    int total_laranjas = 0;

    std::vector<TrackerEntry> tracker;
    int proximo_id = 0;

    // --- Abrir vídeo ---
    capture.open(videofile);
    if (!capture.isOpened()) {
        std::cerr << "Erro ao abrir o ficheiro de video!\n";
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

    // --- Alocar imagens IVC ---
    IVC* image_rgb   = vc_image_new(video.width, video.height, 3, 255);
    IVC* image_hsv   = vc_image_new(video.width, video.height, 3, 255);
    IVC* image_v     = vc_image_new(video.width, video.height, 1, 255);
    IVC* image_v_eq  = vc_image_new(video.width, video.height, 1, 255);
    IVC* image_v_gau = vc_image_new(video.width, video.height, 1, 255);
    IVC* image_seg   = vc_image_new(video.width, video.height, 1, 255);
    IVC* image_tmp   = vc_image_new(video.width, video.height, 1, 255);
    IVC* image_lbl   = vc_image_new(video.width, video.height, 1, 255);

    if (!image_rgb || !image_hsv || !image_v    || !image_v_eq ||
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
        // 1. BGR -> RGB (IVC)
        // -------------------------------------------------------
        bgr_to_ivc_rgb(frame, image_rgb);

        // -------------------------------------------------------
        // 2. RGB -> HSV
        // -------------------------------------------------------
        vc_rgb_to_hsv(image_rgb, image_hsv);

        // -------------------------------------------------------
        // 3. Equalização do histograma do canal V + suavização Gaussian
        //    Normaliza variações de iluminação antes da segmentação.
        // -------------------------------------------------------
        extrair_canal_v(image_hsv, image_v);
        vc_gray_histogram_equalization(image_v, image_v_eq);
        vc_gray_lowpass_gaussian_filter(image_v_eq, image_v_gau);
        repor_canal_v(image_v_gau, image_hsv);

        // -------------------------------------------------------
        // 4. Segmentação HSV — cor laranja
        //    H: 10°–40° (laranjas em RGB->HSV da lib)
        //    S: 150–255 — limiar ALTO para excluir a maçã
        //               (maçã tem S~100-130; laranja tem S~170-255)
        //    V: 60–255  — após equalização
        // -------------------------------------------------------
        vc_hsv_segmentation(image_hsv, image_seg,
                            10, 40,    // Hmin, Hmax (graus)
                            150, 255,  // Smin alto para excluir maca
                            60, 255);  // Vmin, Vmax

        // -------------------------------------------------------
        // 5. Morfologia: Open (remove ruído) + Close (fecha buracos)
        // -------------------------------------------------------
        vc_binary_open (image_seg, image_tmp, 5);
        vc_binary_close(image_tmp, image_seg, 9);

        // -------------------------------------------------------
        // 6. Blob labelling + métricas
        // -------------------------------------------------------
        int nblobs = 0;
        OVC* blobs = vc_binary_blob_labelling(image_seg, image_lbl, &nblobs);

        int laranjas_frame = 0;

        if (blobs != NULL && nblobs > 0) {
            vc_binary_blob_info(image_lbl, blobs, nblobs);

            // ---------------------------------------------------
            // 7. Desenhar rastros (antes das anotações)
            // ---------------------------------------------------
            for (int i = 0; i < (int)tracker.size(); i++) {
                if (tracker[i].frames_ausente == 0)
                    desenhar_rastro(frame, tracker[i]);
            }

            // ---------------------------------------------------
            // 8. Filtrar e anotar blobs válidos
            // ---------------------------------------------------
            for (int i = 0; i < nblobs; i++) {
                if (blobs[i].area < AREA_MIN_LARANJA) continue;

                double circ      = calcular_circularidade(blobs[i].area,
                                                          blobs[i].perimeter);
                bool   irregular = (circ < CIRCULARIDADE_MIN);

                double diametro_px = (blobs[i].width + blobs[i].height) / 2.0;
                double diametro_mm = diametro_px * MM_POR_PIXEL;
                double area_mm2    = blobs[i].area * (MM_POR_PIXEL * MM_POR_PIXEL);
                double perim_mm    = blobs[i].perimeter * MM_POR_PIXEL;
                const char* calibre = classificar_laranja(diametro_mm);

                // Associar ao tracker
                int idx      = tracker_associar(tracker,
                                                blobs[i].xc, blobs[i].yc,
                                                proximo_id);
                int id_laranja = tracker[idx].id;
                cv::Scalar cor_laranja = CORES_RASTRO[id_laranja % N_CORES_RASTRO];

                // -----------------------------------------------
                // CONTAGEM PELA LINHA:
                // Conta apenas quando o centróide cruza Y=LINHA_CONTAGEM
                // de cima para baixo (rastro tem posição anterior).
                // Assim cada laranja é contada exactamente uma vez.
                // -----------------------------------------------
                if (!tracker[idx].ja_contada &&
                    (int)tracker[idx].rastro.size() >= 2) {
                    int y_ant = tracker[idx].rastro[
                        (int)tracker[idx].rastro.size() - 2].y;
                    int y_novo = blobs[i].yc;
                    if (y_ant < LINHA_CONTAGEM && y_novo >= LINHA_CONTAGEM) {
                        tracker[idx].ja_contada = true;
                        total_laranjas++;
                        std::cout << "[Frame " << video.nframe
                                  << "] CONTAGEM: laranja ID=" << id_laranja
                                  << " cruzou a linha! Total=" << total_laranjas << "\n";
                    }
                }

                // Bounding box
                cv::Scalar cor_box = irregular
                    ? cv::Scalar(0, 0, 220)
                    : cor_laranja;
                cv::rectangle(frame,
                    cv::Point(blobs[i].x, blobs[i].y),
                    cv::Point(blobs[i].x + blobs[i].width,
                              blobs[i].y + blobs[i].height),
                    cor_box, 2);

                // Centro de massa
                cv::circle(frame,
                    cv::Point(blobs[i].xc, blobs[i].yc),
                    5, cv::Scalar(0, 0, 255), -1);

                // Etiqueta: calibre
                std::string lbl_cal = irregular ? "IRREGULAR" : std::string(calibre);
                cv::putText(frame, lbl_cal,
                    cv::Point(blobs[i].x, blobs[i].y - 30),
                    cv::FONT_HERSHEY_SIMPLEX, 0.44,
                    cv::Scalar(0, 0, 0), 2);
                cv::putText(frame, lbl_cal,
                    cv::Point(blobs[i].x, blobs[i].y - 30),
                    cv::FONT_HERSHEY_SIMPLEX, 0.44,
                    irregular ? cv::Scalar(0, 0, 255) : cv::Scalar(0, 220, 255), 1);

                // Etiqueta: diâmetro
                std::string lbl_dim = "D:" + std::to_string((int)diametro_mm) + "mm";
                cv::putText(frame, lbl_dim,
                    cv::Point(blobs[i].x, blobs[i].y - 16),
                    cv::FONT_HERSHEY_SIMPLEX, 0.42,
                    cv::Scalar(0, 0, 0), 2);
                cv::putText(frame, lbl_dim,
                    cv::Point(blobs[i].x, blobs[i].y - 16),
                    cv::FONT_HERSHEY_SIMPLEX, 0.42,
                    cv::Scalar(255, 255, 255), 1);

                // Etiqueta: circularidade
                std::string lbl_circ = "C:" + std::to_string((int)(circ * 100)) + "%";
                cv::putText(frame, lbl_circ,
                    cv::Point(blobs[i].x, blobs[i].y - 3),
                    cv::FONT_HERSHEY_SIMPLEX, 0.40,
                    cv::Scalar(0, 0, 0), 2);
                cv::putText(frame, lbl_circ,
                    cv::Point(blobs[i].x, blobs[i].y - 3),
                    cv::FONT_HERSHEY_SIMPLEX, 0.40,
                    cv::Scalar(180, 255, 180), 1);

                laranjas_frame++;

                std::cout << "[Frame " << video.nframe << "]"
                          << "  ID=" << id_laranja
                          << "  Area=" << blobs[i].area
                          << "px (" << (int)area_mm2 << "mm2)"
                          << "  Diam~" << (int)diametro_mm << "mm"
                          << "  Circ=" << (int)(circ * 100) << "%"
                          << (irregular ? " [IRREGULAR]" : "")
                          << "  Calibre=" << calibre
                          << "  Centro=(" << blobs[i].xc << ","
                          << blobs[i].yc << ")"
                          << "\n";
            }

            free(blobs);
        }

        tracker_limpar_ausentes(tracker, 15);

        // -------------------------------------------------------
        // 9. Linha de contagem — cv::line()
        //    Azul claro com pequeno texto indicativo
        // -------------------------------------------------------
        cv::line(frame,
            cv::Point(0,            LINHA_CONTAGEM),
            cv::Point(video.width,  LINHA_CONTAGEM),
            cv::Scalar(255, 200, 0), 2);
        cv::putText(frame, "LINHA DE CONTAGEM",
            cv::Point(video.width - 280, LINHA_CONTAGEM - 6),
            cv::FONT_HERSHEY_SIMPLEX, 0.50,
            cv::Scalar(0, 0, 0), 2);
        cv::putText(frame, "LINHA DE CONTAGEM",
            cv::Point(video.width - 280, LINHA_CONTAGEM - 6),
            cv::FONT_HERSHEY_SIMPLEX, 0.50,
            cv::Scalar(255, 200, 0), 1);

        // -------------------------------------------------------
        // 10. Painel HUD
        // -------------------------------------------------------
        cv::rectangle(frame,
            cv::Point(0, 0), cv::Point(490, 130),
            cv::Scalar(0, 0, 0), -1);

        str = "RESOLUCAO: " + std::to_string(video.width)
            + "x" + std::to_string(video.height);
        cv::putText(frame, str, cv::Point(10, 22),
            cv::FONT_HERSHEY_SIMPLEX, 0.62, cv::Scalar(180, 180, 180), 1);

        str = "FRAME: " + std::to_string(video.nframe)
            + " / " + std::to_string(video.ntotalframes);
        cv::putText(frame, str, cv::Point(10, 44),
            cv::FONT_HERSHEY_SIMPLEX, 0.62, cv::Scalar(180, 180, 180), 1);

        str = "LARANJAS NA FRAME: " + std::to_string(laranjas_frame);
        cv::putText(frame, str, cv::Point(10, 66),
            cv::FONT_HERSHEY_SIMPLEX, 0.62, cv::Scalar(0, 200, 255), 1);

        str = "TOTAL CONTADO (linha): " + std::to_string(total_laranjas);
        cv::putText(frame, str, cv::Point(10, 88),
            cv::FONT_HERSHEY_SIMPLEX, 0.62, cv::Scalar(0, 255, 100), 1);

        str = "ESCALA: 280px=55mm (~0.196mm/px)";
        cv::putText(frame, str, cv::Point(10, 110),
            cv::FONT_HERSHEY_SIMPLEX, 0.50, cv::Scalar(130, 130, 130), 1);

        str = "LEGENDA: cor unica por laranja  vermelho=irregular";
        cv::putText(frame, str, cv::Point(10, 128),
            cv::FONT_HERSHEY_SIMPLEX, 0.44, cv::Scalar(130, 130, 130), 1);

        // -------------------------------------------------------
        // 11. Janelas
        // -------------------------------------------------------
        cv::Mat bin_display(video.height, video.width,
                            CV_8UC1, image_seg->data);
        cv::imshow("VC - BINARIO", bin_display);
        cv::imshow("VC - VIDEO",   frame);

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
    std::cout << "Total de laranjas contadas (cruzaram a linha): "
              << total_laranjas << "\n";
    return 0;
}
