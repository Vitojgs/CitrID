#ifndef TRACKING_HPP
#define TRACKING_HPP

#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/videoio.hpp>
#include <cmath>
#include <vector>

// ---------------------------------------------------------------
// Definições e Constantes do Tracker
// ---------------------------------------------------------------
#define TRACKER_DIST   150
#define TRACKER_ABS    10
#define RASTRO_MAX     20
#define LINHA_Y        360
#define N_CORES        8

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
// Paleta de cores BGR para tracks e bounding boxes
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

// ---------------------------------------------------------------
// Tracker: associa (xc,yc) a uma entrada existente ou cria nova.
// Actualiza o historico do rastro.
// Devolve o indice da entrada no vector.
// ---------------------------------------------------------------
int tracker_associar(std::vector<TrackerEntry>& T, int xc, int yc, int& next_id);


// ---------------------------------------------------------------
// Actualiza ausencias: so incrementa entradas nao vistas.
// Recebe o vector de indices visitados nesta frame.
// ---------------------------------------------------------------
void tracker_tick(std::vector<TrackerEntry>& T, const std::vector<int>& vistos);

// ---------------------------------------------------------------
// Desenha o rastro de trajetoria com cv::line().
// Efeito de desvanecimento: segmentos mais antigos mais escuros.
// So desenha se houver pelo menos 2 pontos distintos.
// ---------------------------------------------------------------
void desenhar_rastro(cv::Mat& frame, const TrackerEntry& e);

#endif