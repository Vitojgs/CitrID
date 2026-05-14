// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//           INSTITUTO POLITÉCNICO DO CÁVADO E DO AVE
//                          2025/2026
//             ENGENHARIA DE SISTEMAS INFORMÁTICOS
//                    VISÃO POR COMPUTADOR
//
//         Trabalho Prático - Deteção e Classificação de Laranjas
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

#include "lib/vc.h"
#include "lib/tracking.hpp"
#include "lib/utils.hpp"

// -------------------------------------------------------.--------+
// MAIN
// ---------------------------------------------------------------+

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
              << "FPS="    << video.fps
              << "Frames=" << video.ntotalframes << "\n";

    cv::namedWindow("VC - VIDEO",   cv::WINDOW_AUTOSIZE);
    cv::namedWindow("VC - BINARIO", cv::WINDOW_AUTOSIZE);

    // -------------------------------------------------------+
    // Alocar imagens IVC
    // -------------------------------------------------------+
    IVC* img_hsv = vc_image_new(video.width, video.height, 3, 255); // HSV
    IVC* img_seg = vc_image_new(video.width, video.height, 1, 255); // binario
    IVC* img_tmp = vc_image_new(video.width, video.height, 1, 255); // morfologia
    IVC* img_lbl = vc_image_new(video.width, video.height, 1, 255); // labels

    if (!img_hsv || !img_seg || !img_tmp || !img_lbl) {
        std::cerr << "Erro ao alocar imagens IVC!\n";
        return 1;
    }

    vc_timer();

    cv::Mat frame;
    while (key != 'q') {
        capture.read(frame);
        if (frame.empty()) break;
        video.nframe = (int)capture.get(cv::CAP_PROP_POS_FRAMES);

        // -------------------------------------------------------+
        // 1 & 2. BGR -> HSV 
        // Uma estrutura IVC temporária que aponta diretamente
        // para a memória do OpenCV. Poupa 1 iteração e muita memória!
        // -------------------------------------------------------+

        IVC img_bgr;
            img_bgr.width = video.width;
            img_bgr.height = video.height;
            img_bgr.channels = 3;
            img_bgr.bytesperline = video.width * 3;
            img_bgr.data = frame.data; 

        vc_bgr_to_hsv(&img_bgr, img_hsv);

        // -------------------------------------------------------+
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
        // -------------------------------------------------------+

        vc_hsv_segmentation(img_hsv, img_seg,
                            12,  34,   // Hmin, Hmax (graus)
                            110, 255,  // Smin, Smax
                            40,  255); // Vmin, Vmax

        // -------------------------------------------------------+
        // 4. Morfologia
        //    Open  kernel=7: remove ruido e reflexos pequenos
        //    Close kernel=25: fecha os buracos internos da laranja
        //    (reflexos na superficie e manchas escuras criam buracos
        //    no blob; um kernel grande garante que ficam fechados,
        //    melhorando a circularidade para 0.70-0.80)
        // -------------------------------------------------------

        vc_binary_open (img_seg, img_tmp, 7);
        vc_binary_close(img_tmp, img_seg, 25);

        // -------------------------------------------------------+
        // 5. Blob labelling + metricas
        // -------------------------------------------------------+
        int nblobs = 0;
        OVC* blobs = vc_binary_blob_labelling(img_seg, img_lbl, &nblobs);

        int laranjas_visiveis = 0;
        std::vector<int> indices_vistos;

        if (blobs != NULL && nblobs > 0) {
            vc_binary_blob_info(img_lbl, blobs, nblobs);

            // ---------------------------------------------------+
            // 6. Rastros de todas as entradas do tracker
            //    (desenhados antes das bounding boxes)
            // ---------------------------------------------------+
            for (int i = 0; i < (int)tracker.size(); i++)
                desenhar_rastro(frame, tracker[i]);

            // ---------------------------------------------------+
            // 7. Processar cada blob
            // ---------------------------------------------------+
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
                int idx = tracker_associar(tracker, blobs[i].xc, blobs[i].yc, next_id);
                indices_vistos.push_back(idx);

                // Contagem pela linha de contagem
                // Laranja so e contada quando cruza Y=LINHA_Y de cima
                // para baixo (movimento ascendente em Y no frame).
                if (!tracker[idx].ja_contada && (int)tracker[idx].rastro.size() >= 2) {
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
                std::string lbl_cal = irregular ? "IRREGULAR" : std::string(cal);
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

        // -------------------------------------------------------+
        // 8. Atualizar tracker (so incrementa ausentes das
        //    entradas nao vistas nesta frame)
        // -------------------------------------------------------+

        tracker_tick(tracker, indices_vistos);

        // -------------------------------------------------------+
        // 9. Linha de contagem  (cv::line)
        //    Desenhada por cima de tudo para ser sempre visivel.
        // -------------------------------------------------------+

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

        // -------------------------------------------------------+
        // 10. Painel HUD
        // -------------------------------------------------------+
        
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

        // -------------------------------------------------------+
        // 11. Janelas
        // -------------------------------------------------------+
        cv::Mat bin_disp(video.height, video.width,
                         CV_8UC1, img_seg->data);
        cv::imshow("VC - BINARIO", bin_disp);
        cv::imshow("VC - VIDEO",   frame);

        key = cv::waitKey(1);
    }

    vc_timer();

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