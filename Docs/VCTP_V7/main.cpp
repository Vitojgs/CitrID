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
// Calibracao (enunciado: 280px = 55mm)
// ---------------------------------------------------------------
#define PIXELS_POR_MM   (280.0 / 55.0)
#define MM_POR_PIXEL    (55.0  / 280.0)

// Area minima: laranjas parcialmente visiveis podem ter area baixa
// Definido empiricamente para aceitar laranjas a entrar no frame.
#define AREA_MIN_LARANJA  15000

// Circularidade: 4*pi*area/perimetro^2.  Circulo perfeito = 1.0.
// Blobs abaixo de REJEITAR sao ignorados (maca circ~0.20-0.43).
// Blobs entre REJEITAR e MIN sao marcados IRREGULAR.
// Laranjas integras: circ~0.70-0.80.
#define CIRC_REJEITAR  0.50
#define CIRC_MIN       0.65

// Tracker: distancia maxima (px) para associar blob a laranja existente.
// Laranjas movem-se ~7px/frame verticalmente -> 150px e muito generoso.
#define TRACKER_DIST   150

// Frames de tolerancia antes de remover entrada do tracker.
#define TRACKER_ABS    10

// Comprimento maximo do rastro de trajetoria (cv::line).
#define RASTRO_MAX     20

// Linha de contagem horizontal.
// Laranjas descem de Y=0 para Y=720, sao contadas ao cruzar esta linha.
#define LINHA_Y        360

// ---------------------------------------------------------------
// Estruturas
// ---------------------------------------------------------------
struct Ponto { int x, y; };

struct TrackerEntry {
    int  xc, yc;
    int  frames_ausente;
    int  id;
    bool ja_contada;
    std::vector<Ponto> rastro;
};

// ---------------------------------------------------------------
// Paleta de cores BGR para rastros e bounding boxes
// ---------------------------------------------------------------
static const cv::Scalar CORES[] = {
    cv::Scalar( 50, 220, 255),
    cv::Scalar(255, 100,  50),
    cv::Scalar( 50, 255, 150),
    cv::Scalar(200,  50, 255),
    cv::Scalar(255, 200,  50),
    cv::Scalar( 50,  50, 255),
    cv::Scalar(150, 255,  50),
    cv::Scalar(255,  50, 200),
};
#define N_CORES 8

// ---------------------------------------------------------------
// Classificacao segundo Regulamento CEE n. 379/71
// ---------------------------------------------------------------
const char* classificar(double dmm) {
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
// Circularidade: 4*pi*area / perimetro^2
// ---------------------------------------------------------------
double circularidade(int area, int perim) {
    if (perim == 0) return 0.0;
    return (4.0 * M_PI * (double)area) / ((double)perim * (double)perim);
}

// ---------------------------------------------------------------
// Timer (identico ao CodigoExemplo.cpp)
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
        std::cout << "Tempo decorrido: " << ts.count() << "s\n";
        std::cout << "Pressione qualquer tecla para continuar...\n";
        std::cin.get();
    }
}

// ---------------------------------------------------------------
// BGR (OpenCV) -> RGB (IVC)
// vc_rgb_to_hsv espera canais R,G,B nas posicoes 0,1,2.
// ---------------------------------------------------------------
void bgr_to_ivc_rgb(const cv::Mat& frame, IVC* ivc) {
    const unsigned char* src = frame.data;
    unsigned char*       dst = ivc->data;
    int W = frame.cols, H = frame.rows;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            long int p = (long int)(y * W * 3) + x * 3;
            dst[p    ] = src[p + 2]; // R
            dst[p + 1] = src[p + 1]; // G
            dst[p + 2] = src[p    ]; // B
        }
}

// ---------------------------------------------------------------
// Tracker: associa (xc,yc) a uma entrada existente ou cria nova.
// Actualiza o historico do rastro.
// Devolve o indice da entrada no vector.
// ---------------------------------------------------------------
int tracker_associar(std::vector<TrackerEntry>& T,
                     int xc, int yc, int& next_id) {
    double dmin = 1e9;
    int    imin = -1;
    for (int i = 0; i < (int)T.size(); i++) {
        double dx = xc - T[i].xc, dy = yc - T[i].yc;
        double d  = sqrt(dx*dx + dy*dy);
        if (d < dmin) { dmin = d; imin = i; }
    }
    if (imin >= 0 && dmin <= TRACKER_DIST) {
        T[imin].xc             = xc;
        T[imin].yc             = yc;
        T[imin].frames_ausente = 0;
        T[imin].rastro.push_back({xc, yc});
        if ((int)T[imin].rastro.size() > RASTRO_MAX)
            T[imin].rastro.erase(T[imin].rastro.begin());
        return imin;
    }
    TrackerEntry e;
    e.xc = xc; e.yc = yc; e.frames_ausente = 0;
    e.id = next_id++; e.ja_contada = false;
    e.rastro.push_back({xc, yc});
    T.push_back(e);
    return (int)T.size() - 1;
}

// ---------------------------------------------------------------
// Actualiza ausencias: so incrementa entradas nao vistas.
// Recebe o vector de indices visitados nesta frame.
// ---------------------------------------------------------------
void tracker_tick(std::vector<TrackerEntry>& T,
                  const std::vector<int>& vistos) {
    std::vector<bool> visto(T.size(), false);
    for (int idx : vistos)
        if (idx >= 0 && idx < (int)T.size()) visto[idx] = true;
    for (int i = (int)T.size() - 1; i >= 0; i--) {
        if (!visto[i]) {
            T[i].frames_ausente++;
            if (T[i].frames_ausente > TRACKER_ABS)
                T.erase(T.begin() + i);
        }
    }
}

// ---------------------------------------------------------------
// Desenha o rastro de trajetoria com cv::line().
// Efeito de desvanecimento: segmentos mais antigos mais escuros.
// So desenha se houver pelo menos 2 pontos distintos.
// ---------------------------------------------------------------
void desenhar_rastro(cv::Mat& frame, const TrackerEntry& e) {
    int n = (int)e.rastro.size();
    if (n < 2) return;
    cv::Scalar cb = CORES[e.id % N_CORES];
    for (int i = 1; i < n; i++) {
        // Nao desenhar segmento se os dois pontos sao identicos
        if (e.rastro[i].x == e.rastro[i-1].x &&
            e.rastro[i].y == e.rastro[i-1].y) continue;
        double f  = (double)i / (double)(n - 1);
        int    th = (int)(1.0 + f * 2.0);
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
    int total_contado = 0;
    std::vector<TrackerEntry> tracker;
    int next_id = 0;

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

    cv::namedWindow("VC - VIDEO",   cv::WINDOW_AUTOSIZE);
    cv::namedWindow("VC - BINARIO", cv::WINDOW_AUTOSIZE);

    // -------------------------------------------------------
    // Alocar imagens IVC
    // -------------------------------------------------------
    IVC* img_rgb = vc_image_new(video.width, video.height, 3, 255); // BGR->RGB
    IVC* img_hsv = vc_image_new(video.width, video.height, 3, 255); // HSV
    IVC* img_seg = vc_image_new(video.width, video.height, 1, 255); // binario
    IVC* img_tmp = vc_image_new(video.width, video.height, 1, 255); // morfologia
    IVC* img_lbl = vc_image_new(video.width, video.height, 1, 255); // labels

    if (!img_rgb || !img_hsv || !img_seg || !img_tmp || !img_lbl) {
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
        //    O OpenCV entrega BGR; vc_rgb_to_hsv espera RGB.
        // -------------------------------------------------------
        bgr_to_ivc_rgb(frame, img_rgb);

        // -------------------------------------------------------
        // 2. RGB -> HSV  (lib vc.c)
        //    H em graus [0,360], S e V em [0,255].
        // -------------------------------------------------------
        vc_rgb_to_hsv(img_rgb, img_hsv);

        // -------------------------------------------------------
        // 3. Segmentacao HSV - cor laranja
        //
        //    Analise empirica dos pixels do video:
        //      Laranja: H~15-35 graus, S~100-255, V~80-255
        //      Maca:    H~0-20 graus,  S~50-220   (parcialmente sobrepostos)
        //
        //    Estrategia em duas camadas:
        //      a) Range HSV captura laranjas com margem suficiente
        //      b) Circularidade elimina a maca (circ~0.20-0.43 < CIRC_REJEITAR)
        //
        //    Nota: equalizacao do histograma foi testada e REJEITADA -
        //    amplifica o contraste interior da laranja criando buracos
        //    que reduzem drasticamente a circularidade para 0.50-0.60.
        // -------------------------------------------------------
        vc_hsv_segmentation(img_hsv, img_seg,
                            15,  35,   // Hmin, Hmax (graus)
                            100, 255,  // Smin, Smax
                            80,  255); // Vmin, Vmax

        // -------------------------------------------------------
        // 4. Morfologia
        //    Open  kernel=7: remove ruido e reflexos pequenos
        //    Close kernel=25: fecha os buracos internos da laranja
        //    (reflexos na superficie e manchas escuras criam buracos
        //    no blob; um kernel grande garante que ficam fechados,
        //    melhorando a circularidade para 0.70-0.80)
        // -------------------------------------------------------
        vc_binary_open (img_seg, img_tmp, 7);
        vc_binary_close(img_tmp, img_seg, 25);

        // -------------------------------------------------------
        // 5. Blob labelling + metricas
        // -------------------------------------------------------
        int nblobs = 0;
        OVC* blobs = vc_binary_blob_labelling(img_seg, img_lbl, &nblobs);

        int laranjas_visiveis = 0;
        std::vector<int> indices_vistos;

        if (blobs != NULL && nblobs > 0) {
            vc_binary_blob_info(img_lbl, blobs, nblobs);

            // ---------------------------------------------------
            // 6. Rastros de todas as entradas do tracker
            //    (desenhados antes das bounding boxes)
            // ---------------------------------------------------
            for (int i = 0; i < (int)tracker.size(); i++)
                desenhar_rastro(frame, tracker[i]);

            // ---------------------------------------------------
            // 7. Processar cada blob
            // ---------------------------------------------------
            for (int i = 0; i < nblobs; i++) {

                // Filtro de area minima
                if (blobs[i].area < AREA_MIN_LARANJA) continue;

                // Circularidade
                double circ = circularidade(blobs[i].area,
                                            blobs[i].perimeter);

                // Rejeitar completamente blobs nao-circulares (maca)
                // Medido no video: maca circ~0.20-0.43 < CIRC_REJEITAR=0.50
                if (circ < CIRC_REJEITAR) continue;

                bool irregular = (circ < CIRC_MIN);

                // Metricas de tamanho
                double dpx = (blobs[i].width + blobs[i].height) / 2.0;
                double dmm = dpx * MM_POR_PIXEL;
                double area_mm2 = blobs[i].area * MM_POR_PIXEL * MM_POR_PIXEL;
                double peri_mm  = blobs[i].perimeter * MM_POR_PIXEL;
                const char* cal = classificar(dmm);

                // Tracker
                int idx = tracker_associar(tracker,
                                           blobs[i].xc, blobs[i].yc,
                                           next_id);
                indices_vistos.push_back(idx);

                // Contagem pela linha de contagem
                // Laranja so e contada quando cruza Y=LINHA_Y de cima
                // para baixo (movimento ascendente em Y no frame).
                if (!tracker[idx].ja_contada &&
                    (int)tracker[idx].rastro.size() >= 2) {
                    int y_ant  = tracker[idx].rastro[
                        (int)tracker[idx].rastro.size() - 2].y;
                    int y_novo = tracker[idx].yc;
                    if (y_ant < LINHA_Y && y_novo >= LINHA_Y) {
                        tracker[idx].ja_contada = true;
                        total_contado++;
                        std::cout << "[F" << video.nframe
                                  << "] CONTAGEM ID=" << tracker[idx].id
                                  << " Total=" << total_contado << "\n";
                    }
                }

                // Cor desta laranja (consistente com o rastro)
                cv::Scalar cor = CORES[tracker[idx].id % N_CORES];

                // Bounding box
                cv::rectangle(frame,
                    cv::Point(blobs[i].x, blobs[i].y),
                    cv::Point(blobs[i].x + blobs[i].width,
                              blobs[i].y + blobs[i].height),
                    irregular ? cv::Scalar(0, 0, 220) : cor, 2);

                // Centro de massa
                cv::circle(frame,
                    cv::Point(blobs[i].xc, blobs[i].yc),
                    5, cv::Scalar(0, 0, 255), -1);

                // Etiqueta calibre
                std::string lbl_cal = irregular
                    ? "IRREGULAR" : std::string(cal);
                cv::putText(frame, lbl_cal,
                    cv::Point(blobs[i].x, blobs[i].y - 30),
                    cv::FONT_HERSHEY_SIMPLEX, 0.44,
                    cv::Scalar(0, 0, 0), 2);
                cv::putText(frame, lbl_cal,
                    cv::Point(blobs[i].x, blobs[i].y - 30),
                    cv::FONT_HERSHEY_SIMPLEX, 0.44,
                    irregular ? cv::Scalar(0, 0, 255) : cv::Scalar(0, 220, 255), 1);

                // Etiqueta diametro
                std::string lbl_d = "D:" + std::to_string((int)dmm) + "mm";
                cv::putText(frame, lbl_d,
                    cv::Point(blobs[i].x, blobs[i].y - 16),
                    cv::FONT_HERSHEY_SIMPLEX, 0.42,
                    cv::Scalar(0, 0, 0), 2);
                cv::putText(frame, lbl_d,
                    cv::Point(blobs[i].x, blobs[i].y - 16),
                    cv::FONT_HERSHEY_SIMPLEX, 0.42,
                    cv::Scalar(255, 255, 255), 1);

                // Etiqueta circularidade
                std::string lbl_c = "C:" + std::to_string((int)(circ*100)) + "%";
                cv::putText(frame, lbl_c,
                    cv::Point(blobs[i].x, blobs[i].y - 3),
                    cv::FONT_HERSHEY_SIMPLEX, 0.40,
                    cv::Scalar(0, 0, 0), 2);
                cv::putText(frame, lbl_c,
                    cv::Point(blobs[i].x, blobs[i].y - 3),
                    cv::FONT_HERSHEY_SIMPLEX, 0.40,
                    cv::Scalar(180, 255, 180), 1);

                laranjas_visiveis++;

                std::cout << "[F" << video.nframe
                          << "] ID=" << tracker[idx].id
                          << " A=" << blobs[i].area
                          << "px (" << (int)area_mm2 << "mm2)"
                          << " P=" << blobs[i].perimeter
                          << "px (" << (int)peri_mm << "mm)"
                          << " D~" << (int)dmm << "mm"
                          << " C=" << (int)(circ*100) << "%"
                          << (irregular ? " [IRR]" : "")
                          << " " << cal
                          << " ctr=(" << blobs[i].xc
                          << "," << blobs[i].yc << ")\n";
            }

            free(blobs);
        }

        // -------------------------------------------------------
        // 8. Actualizar tracker (so incrementa ausentes das
        //    entradas nao vistas nesta frame)
        // -------------------------------------------------------
        tracker_tick(tracker, indices_vistos);

        // -------------------------------------------------------
        // 9. Linha de contagem  (cv::line)
        //    Desenhada por cima de tudo para ser sempre visivel.
        // -------------------------------------------------------
        cv::line(frame,
            cv::Point(0,           LINHA_Y),
            cv::Point(video.width, LINHA_Y),
            cv::Scalar(0, 255, 255), 2);

        cv::putText(frame, "LINHA DE CONTAGEM",
            cv::Point(video.width - 290, LINHA_Y - 8),
            cv::FONT_HERSHEY_SIMPLEX, 0.50, cv::Scalar(0, 0, 0), 2);
        cv::putText(frame, "LINHA DE CONTAGEM",
            cv::Point(video.width - 290, LINHA_Y - 8),
            cv::FONT_HERSHEY_SIMPLEX, 0.50, cv::Scalar(0, 255, 255), 1);

        // -------------------------------------------------------
        // 10. Painel HUD
        // -------------------------------------------------------
        cv::rectangle(frame,
            cv::Point(0, 0), cv::Point(480, 132),
            cv::Scalar(0, 0, 0), -1);

        str = "RESOLUCAO: " + std::to_string(video.width)
            + "x" + std::to_string(video.height);
        cv::putText(frame, str, cv::Point(10, 22),
            cv::FONT_HERSHEY_SIMPLEX, 0.62, cv::Scalar(180,180,180), 1);

        str = "FRAME: " + std::to_string(video.nframe)
            + " / " + std::to_string(video.ntotalframes);
        cv::putText(frame, str, cv::Point(10, 44),
            cv::FONT_HERSHEY_SIMPLEX, 0.62, cv::Scalar(180,180,180), 1);

        str = "LARANJAS VISIVEIS: " + std::to_string(laranjas_visiveis);
        cv::putText(frame, str, cv::Point(10, 66),
            cv::FONT_HERSHEY_SIMPLEX, 0.62, cv::Scalar(0,200,255), 1);

        str = "TOTAL CONTADO: " + std::to_string(total_contado);
        cv::putText(frame, str, cv::Point(10, 88),
            cv::FONT_HERSHEY_SIMPLEX, 0.62, cv::Scalar(0,255,100), 1);

        str = "ESCALA: 280px=55mm (~0.196mm/px)";
        cv::putText(frame, str, cv::Point(10, 110),
            cv::FONT_HERSHEY_SIMPLEX, 0.50, cv::Scalar(130,130,130), 1);

        str = "LEGENDA: cor unica/laranja  vermelho=irregular";
        cv::putText(frame, str, cv::Point(10, 128),
            cv::FONT_HERSHEY_SIMPLEX, 0.44, cv::Scalar(130,130,130), 1);

        // -------------------------------------------------------
        // 11. Janelas
        // -------------------------------------------------------
        cv::Mat bin_disp(video.height, video.width,
                         CV_8UC1, img_seg->data);
        cv::imshow("VC - BINARIO", bin_disp);
        cv::imshow("VC - VIDEO",   frame);

        key = cv::waitKey(1);
    }

    vc_timer();

    vc_image_free(img_rgb);
    vc_image_free(img_hsv);
    vc_image_free(img_seg);
    vc_image_free(img_tmp);
    vc_image_free(img_lbl);

    cv::destroyAllWindows();
    capture.release();

    std::cout << "\n=== FIM ===\n";
    std::cout << "Total laranjas contadas: " << total_contado << "\n";
    return 0;
}
