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

// Área mínima em px para aceitar um blob.
// Diâmetro mínimo legal 53mm → raio≈135px → área≈57000px.
// Valor conservador para aceitar laranjas parcialmente visíveis.
#define AREA_MIN_LARANJA  15000

// Circularidade mínima: 4*π*área/perímetro².
// 1.0 = círculo perfeito. Abaixo de 0.55 → marcado IRREGULAR.
#define CIRCULARIDADE_MIN 0.55

// Distância máxima (px) entre centróides de frames consecutivas
// para associar um blob à mesma laranja no tracker.
// Valor generoso porque as laranjas sobem de Y~0 para Y~720 ao
// longo de ~100 frames → ~7px/frame de movimento vertical.
#define TRACKER_DIST_MAX  150

// Número de frames de tolerância antes de remover uma entrada
// do tracker quando a laranja deixa de ser detetada.
#define TRACKER_MAX_AUSENTE 10

// Comprimento máximo do histórico de rastro (cv::line).
// 20 posições a 25 fps ≈ 0.8 s de trajetória visível.
#define RASTRO_MAX_POS    20

// Linha de contagem horizontal em Y=360 (meio do frame 720px).
// As laranjas descem de Y≈0 para Y≈720, portanto são contadas
// quando o centróide cruza esta linha de cima para baixo.
#define LINHA_Y           360

// ---------------------------------------------------------------
// Estruturas
// ---------------------------------------------------------------
struct Ponto { int x, y; };

struct TrackerEntry {
    int  xc, yc;                // centróide na última frame vista
    int  frames_ausente;        // frames consecutivas sem correspondência
    int  id;                    // ID único desta laranja
    bool ja_contada;            // true após cruzar LINHA_Y
    std::vector<Ponto> rastro;  // histórico para cv::line (rastro)
};

// ---------------------------------------------------------------
// Paleta de cores (BGR) — cada laranja recebe cor pelo id % 8
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
#define N_CORES 8

// ---------------------------------------------------------------
// Classificação — Regulamento CEE nº 379/71
// ---------------------------------------------------------------
const char* classificar_laranja(double dmm) {
    if      (dmm >= 100) return "CAL 0 (>=100mm)";
    else if (dmm >=  87) return "CAL 1 (87-100mm)";
    else if (dmm >=  84) return "CAL 2 (84-96mm)";
    else if (dmm >=  81) return "CAL 3 (81-92mm)";
    else if (dmm >=  77) return "CAL 4 (77-88mm)";
    else if (dmm >=  73) return "CAL 5 (73-84mm)";
    else if (dmm >=  70) return "CAL 6 (70-80mm)";
    else if (dmm >=  67) return "CAL 7 (67-76mm)";
    else if (dmm >=  64) return "CAL 8 (64-73mm)";
    else if (dmm >=  62) return "CAL 9 (62-70mm)";
    else if (dmm >=  60) return "CAL 10 (60-68mm)";
    else if (dmm >=  58) return "CAL 11 (58-66mm)";
    else if (dmm >=  56) return "CAL 12 (56-63mm)";
    else if (dmm >=  53) return "CAL 13 (53-60mm)";
    else                 return "< MIN (<53mm)";
}

// ---------------------------------------------------------------
// Circularidade: 4*π*area / perimetro^2
// ---------------------------------------------------------------
double calcular_circularidade(int area, int perimeter) {
    if (perimeter == 0) return 0.0;
    return (4.0 * M_PI * (double)area) / ((double)perimeter * (double)perimeter);
}

// ---------------------------------------------------------------
// Timer (idêntico ao CodigoExemplo.cpp)
// ---------------------------------------------------------------
void vc_timer(void) {
    static bool running = false;
    static std::chrono::steady_clock::time_point t0 =
        std::chrono::steady_clock::now();
    if (!running) {
        running = true;
    } else {
        std::chrono::duration<double> ts =
            std::chrono::duration_cast<std::chrono::duration<double>>(
                std::chrono::steady_clock::now() - t0);
        std::cout << "Tempo decorrido: " << ts.count() << " segundos\n";
        std::cout << "Pressione qualquer tecla para continuar...\n";
        std::cin.get();
    }
}

// ---------------------------------------------------------------
// BGR (OpenCV) → RGB (IVC)
// vc_rgb_to_hsv espera canais na ordem R, G, B (posições 0,1,2).
// ---------------------------------------------------------------
void bgr_to_ivc_rgb(const cv::Mat& frame, IVC* ivc) {
    const unsigned char* src = frame.data;
    unsigned char*       dst = ivc->data;
    int W = frame.cols, H = frame.rows;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            long int p = (long int)(y * W * 3) + (x * 3);
            dst[p    ] = src[p + 2]; // R
            dst[p + 1] = src[p + 1]; // G
            dst[p + 2] = src[p    ]; // B
        }
}

// ---------------------------------------------------------------
// Extrai canal V (índice 2) do IVC HSV → IVC grayscale 1 canal.
// ---------------------------------------------------------------
void extrair_canal_v(IVC* hsv, IVC* gv) {
    int W = hsv->width, H = hsv->height;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            gv->data[(long int)(y*W)+x] =
                hsv->data[(long int)(y*W*3)+(x*3)+2];
}

// ---------------------------------------------------------------
// Repõe o canal V processado de volta no IVC HSV.
// ---------------------------------------------------------------
void repor_canal_v(IVC* gv, IVC* hsv) {
    int W = hsv->width, H = hsv->height;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            hsv->data[(long int)(y*W*3)+(x*3)+2] =
                gv->data[(long int)(y*W)+x];
}

// ---------------------------------------------------------------
// Tracker: associa (xc,yc) a uma entrada existente ou cria nova.
//
// LÓGICA CORRETA:
//   - Percorre TODAS as entradas e encontra a de menor distância.
//   - Se dist ≤ TRACKER_DIST_MAX  → laranja conhecida, actualiza.
//   - Caso contrário              → nova laranja, cria entrada.
//   - O campo frames_ausente é gerido SEPARADAMENTE em
//     tracker_tick_ausentes(), que só incrementa as entradas que
//     NÃO foram vistas nesta frame (conjunto "matched" passado).
//
// Devolve o índice da entrada no vector (nova ou existente).
// ---------------------------------------------------------------
int tracker_associar(std::vector<TrackerEntry>& tracker,
                     int xc, int yc, int& proximo_id) {
    double dist_min = 1e9;
    int    idx_min  = -1;
    for (int i = 0; i < (int)tracker.size(); i++) {
        double dx = xc - tracker[i].xc;
        double dy = yc - tracker[i].yc;
        double d  = sqrt(dx*dx + dy*dy);
        if (d < dist_min) { dist_min = d; idx_min = i; }
    }
    if (idx_min >= 0 && dist_min <= TRACKER_DIST_MAX) {
        // Laranja conhecida — actualiza posição e rastro
        tracker[idx_min].xc             = xc;
        tracker[idx_min].yc             = yc;
        tracker[idx_min].frames_ausente = 0;   // vista nesta frame
        Ponto p = {xc, yc};
        tracker[idx_min].rastro.push_back(p);
        if ((int)tracker[idx_min].rastro.size() > RASTRO_MAX_POS)
            tracker[idx_min].rastro.erase(tracker[idx_min].rastro.begin());
        return idx_min;
    }
    // Nova laranja
    TrackerEntry e;
    e.xc             = xc;
    e.yc             = yc;
    e.frames_ausente = 0;
    e.id             = proximo_id++;
    e.ja_contada     = false;
    e.rastro.push_back({xc, yc});
    tracker.push_back(e);
    return (int)tracker.size() - 1;
}

// ---------------------------------------------------------------
// Incrementa frames_ausente apenas das entradas NÃO vistas e
// remove as que ultrapassaram TRACKER_MAX_AUSENTE.
// DEVE ser chamado com o conjunto de índices visitados nesta frame.
// ---------------------------------------------------------------
void tracker_tick(std::vector<TrackerEntry>& tracker,
                  const std::vector<int>& indices_vistos) {
    // Construir set de índices vistos (acesso O(1))
    // Como os índices podem mudar após erase, percorremos de trás
    // para a frente para preservar os índices ainda não processados.

    // Primeiro: marcar quais foram vistos
    std::vector<bool> visto(tracker.size(), false);
    for (int idx : indices_vistos)
        if (idx >= 0 && idx < (int)tracker.size())
            visto[idx] = true;

    // De trás para a frente: incrementar ausentes e remover expirados
    for (int i = (int)tracker.size() - 1; i >= 0; i--) {
        if (!visto[i]) {
            tracker[i].frames_ausente++;
            if (tracker[i].frames_ausente > TRACKER_MAX_AUSENTE)
                tracker.erase(tracker.begin() + i);
        }
    }
}

// ---------------------------------------------------------------
// Desenha o rastro de trajetória usando cv::line().
// Segmentos mais antigos → mais escuros e finos (desvanecimento).
// ---------------------------------------------------------------
void desenhar_rastro(cv::Mat& frame, const TrackerEntry& e) {
    int n = (int)e.rastro.size();
    if (n < 2) return;
    cv::Scalar cb = CORES_RASTRO[e.id % N_CORES];
    for (int i = 1; i < n; i++) {
        double f  = (double)i / (double)(n - 1); // 0=antigo, 1=recente
        int    th = (int)(1.0 + f * 2.0);         // 1–3 px
        cv::line(frame,
            cv::Point(e.rastro[i-1].x, e.rastro[i-1].y),
            cv::Point(e.rastro[i  ].x, e.rastro[i  ].y),
            cv::Scalar(cb[0]*f, cb[1]*f, cb[2]*f), th);
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

    int total_laranjas = 0;          // laranjas que cruzaram LINHA_Y
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

    // --- Alocar imagens IVC (reutilizadas por frame) ---
    IVC* image_rgb   = vc_image_new(video.width, video.height, 3, 255);
    IVC* image_hsv   = vc_image_new(video.width, video.height, 3, 255);
    IVC* image_v     = vc_image_new(video.width, video.height, 1, 255);
    IVC* image_v_eq  = vc_image_new(video.width, video.height, 1, 255);
    IVC* image_v_gau = vc_image_new(video.width, video.height, 1, 255);
    IVC* image_seg   = vc_image_new(video.width, video.height, 1, 255);
    IVC* image_tmp   = vc_image_new(video.width, video.height, 1, 255);
    IVC* image_lbl   = vc_image_new(video.width, video.height, 1, 255);

    if (!image_rgb  || !image_hsv || !image_v    || !image_v_eq ||
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
        // 2. RGB → HSV (lib)
        // -------------------------------------------------------
        vc_rgb_to_hsv(image_rgb, image_hsv);

        // -------------------------------------------------------
        // 3. Equalização do canal V + suavização Gaussiana
        //    Normaliza variações de iluminação antes de segmentar.
        // -------------------------------------------------------
        extrair_canal_v(image_hsv, image_v);
        vc_gray_histogram_equalization(image_v, image_v_eq);
        vc_gray_lowpass_gaussian_filter(image_v_eq, image_v_gau);
        repor_canal_v(image_v_gau, image_hsv);

        // -------------------------------------------------------
        // 4. Segmentação HSV
        //    H: 10°–40°  (gama do laranja no espaço da lib)
        //    S: 150–255  (ALTO para excluir a maçã, que tem S~100-130)
        //    V: 60–255   (após equalização, limiar baixo é suficiente)
        // -------------------------------------------------------
        vc_hsv_segmentation(image_hsv, image_seg,
                            10, 40,    // Hmin, Hmax (graus)
                            150, 255,  // Smin alto → exclui maçã
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

        // Índices do tracker visitados nesta frame (para tracker_tick)
        std::vector<int> indices_vistos;

        if (blobs != NULL && nblobs > 0) {
            vc_binary_blob_info(image_lbl, blobs, nblobs);

            // ---------------------------------------------------
            // 7. Rastros de todas as entradas activas (antes das
            //    bounding boxes para ficarem por baixo)
            // ---------------------------------------------------
            for (int i = 0; i < (int)tracker.size(); i++)
                desenhar_rastro(frame, tracker[i]);

            // ---------------------------------------------------
            // 8. Processar cada blob válido
            // ---------------------------------------------------
            for (int i = 0; i < nblobs; i++) {
                if (blobs[i].area < AREA_MIN_LARANJA) continue;

                // Circularidade
                double circ      = calcular_circularidade(blobs[i].area,
                                                          blobs[i].perimeter);
                bool   irregular = (circ < CIRCULARIDADE_MIN);

                // Métricas
                double dpx = (blobs[i].width + blobs[i].height) / 2.0;
                double dmm = dpx * MM_POR_PIXEL;
                const char* calibre = classificar_laranja(dmm);

                // --- Tracker ---
                int idx = tracker_associar(tracker,
                                           blobs[i].xc, blobs[i].yc,
                                           proximo_id);
                indices_vistos.push_back(idx);

                // --- Contagem pela linha ---
                // Condição: y anterior < LINHA_Y e y actual >= LINHA_Y
                // (movimento de cima para baixo, y cresce)
                if (!tracker[idx].ja_contada &&
                    (int)tracker[idx].rastro.size() >= 2) {
                    int y_ant  = tracker[idx].rastro[
                        (int)tracker[idx].rastro.size() - 2].y;
                    int y_novo = tracker[idx].yc;
                    if (y_ant < LINHA_Y && y_novo >= LINHA_Y) {
                        tracker[idx].ja_contada = true;
                        total_laranjas++;
                        std::cout << "[Frame " << video.nframe
                                  << "] CONTAGEM ID=" << tracker[idx].id
                                  << " cruzou linha! Total=" << total_laranjas
                                  << "\n";
                    }
                }

                // --- Cor desta laranja ---
                cv::Scalar cor = CORES_RASTRO[tracker[idx].id % N_CORES];

                // --- Bounding box ---
                cv::rectangle(frame,
                    cv::Point(blobs[i].x, blobs[i].y),
                    cv::Point(blobs[i].x + blobs[i].width,
                              blobs[i].y + blobs[i].height),
                    irregular ? cv::Scalar(0, 0, 220) : cor, 2);

                // --- Centro de massa ---
                cv::circle(frame,
                    cv::Point(blobs[i].xc, blobs[i].yc),
                    5, cv::Scalar(0, 0, 255), -1);

                // --- Etiquetas ---
                std::string lbl_cal = irregular
                    ? "IRREGULAR" : std::string(calibre);
                cv::putText(frame, lbl_cal,
                    cv::Point(blobs[i].x, blobs[i].y - 30),
                    cv::FONT_HERSHEY_SIMPLEX, 0.44,
                    cv::Scalar(0,0,0), 2);
                cv::putText(frame, lbl_cal,
                    cv::Point(blobs[i].x, blobs[i].y - 30),
                    cv::FONT_HERSHEY_SIMPLEX, 0.44,
                    irregular ? cv::Scalar(0,0,255) : cv::Scalar(0,220,255), 1);

                std::string lbl_dim = "D:" + std::to_string((int)dmm) + "mm";
                cv::putText(frame, lbl_dim,
                    cv::Point(blobs[i].x, blobs[i].y - 16),
                    cv::FONT_HERSHEY_SIMPLEX, 0.42, cv::Scalar(0,0,0), 2);
                cv::putText(frame, lbl_dim,
                    cv::Point(blobs[i].x, blobs[i].y - 16),
                    cv::FONT_HERSHEY_SIMPLEX, 0.42, cv::Scalar(255,255,255), 1);

                std::string lbl_circ = "C:" + std::to_string((int)(circ*100)) + "%";
                cv::putText(frame, lbl_circ,
                    cv::Point(blobs[i].x, blobs[i].y - 3),
                    cv::FONT_HERSHEY_SIMPLEX, 0.40, cv::Scalar(0,0,0), 2);
                cv::putText(frame, lbl_circ,
                    cv::Point(blobs[i].x, blobs[i].y - 3),
                    cv::FONT_HERSHEY_SIMPLEX, 0.40, cv::Scalar(180,255,180), 1);

                laranjas_frame++;

                std::cout << "[F" << video.nframe << "]"
                          << " ID=" << tracker[idx].id
                          << " Area=" << blobs[i].area
                          << " Diam~" << (int)dmm << "mm"
                          << " Circ=" << (int)(circ*100) << "%"
                          << (irregular ? " [IRREGULAR]" : "")
                          << " Calibre=" << calibre
                          << " Centro=(" << blobs[i].xc
                          << "," << blobs[i].yc << ")"
                          << "\n";
            }
            free(blobs);
        }

        // -------------------------------------------------------
        // 9. Actualizar tracker: só incrementa ausentes para
        //    entradas QUE NÃO foram vistas nesta frame.
        //    (Bug anterior: incrementava todas, incluindo as vistas)
        // -------------------------------------------------------
        tracker_tick(tracker, indices_vistos);

        // -------------------------------------------------------
        // 10. Linha de contagem — cv::line()
        //     Desenhada em CIMA de tudo o resto para ser sempre visível.
        //     Cor ciano, largura 2px, com legenda.
        // -------------------------------------------------------
        cv::line(frame,
            cv::Point(0,            LINHA_Y),
            cv::Point(video.width,  LINHA_Y),
            cv::Scalar(255, 200, 0), 2);

        // Sombra preta para legibilidade
        cv::putText(frame, "LINHA DE CONTAGEM",
            cv::Point(video.width - 285, LINHA_Y - 7),
            cv::FONT_HERSHEY_SIMPLEX, 0.50, cv::Scalar(0,0,0), 2);
        cv::putText(frame, "LINHA DE CONTAGEM",
            cv::Point(video.width - 285, LINHA_Y - 7),
            cv::FONT_HERSHEY_SIMPLEX, 0.50, cv::Scalar(255,200,0), 1);

        // -------------------------------------------------------
        // 11. Painel HUD
        // -------------------------------------------------------
        cv::rectangle(frame,
            cv::Point(0,0), cv::Point(490, 130),
            cv::Scalar(0,0,0), -1);

        str = "RESOLUCAO: " + std::to_string(video.width)
            + "x" + std::to_string(video.height);
        cv::putText(frame, str, cv::Point(10, 22),
            cv::FONT_HERSHEY_SIMPLEX, 0.62, cv::Scalar(180,180,180), 1);

        str = "FRAME: " + std::to_string(video.nframe)
            + " / " + std::to_string(video.ntotalframes);
        cv::putText(frame, str, cv::Point(10, 44),
            cv::FONT_HERSHEY_SIMPLEX, 0.62, cv::Scalar(180,180,180), 1);

        str = "LARANJAS VISIVEIS: " + std::to_string(laranjas_frame);
        cv::putText(frame, str, cv::Point(10, 66),
            cv::FONT_HERSHEY_SIMPLEX, 0.62, cv::Scalar(0,200,255), 1);

        str = "TOTAL CONTADO: " + std::to_string(total_laranjas);
        cv::putText(frame, str, cv::Point(10, 88),
            cv::FONT_HERSHEY_SIMPLEX, 0.62, cv::Scalar(0,255,100), 1);

        str = "ESCALA: 280px=55mm (~0.196mm/px)";
        cv::putText(frame, str, cv::Point(10, 110),
            cv::FONT_HERSHEY_SIMPLEX, 0.50, cv::Scalar(130,130,130), 1);

        str = "LEGENDA: cor unica/laranja  vermelho=irregular";
        cv::putText(frame, str, cv::Point(10, 128),
            cv::FONT_HERSHEY_SIMPLEX, 0.44, cv::Scalar(130,130,130), 1);

        // -------------------------------------------------------
        // 12. Janelas
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
    std::cout << "Total de laranjas contadas: " << total_laranjas << "\n";
    return 0;
}
