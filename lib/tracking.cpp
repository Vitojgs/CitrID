#include "tracking.hpp"

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