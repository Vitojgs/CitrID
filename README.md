# CitrID — Citrus Identification & Classification System

![Language](https://img.shields.io/badge/language-C%2B%2B%20%7C%20C-blue)
![OpenCV](https://img.shields.io/badge/OpenCV-4.x-green)
![Platform](https://img.shields.io/badge/platform-Windows-lightgrey)
![Status](https://img.shields.io/badge/status-Academic%20Project-orange)

Sistema de **Visão por Computador** para deteção, contagem e classificação de laranjas em ambiente industrial, utilizando processamento de imagem em tempo real.

---

## Overview

O **CitrID** processa vídeo de uma passadeira rolante para:

- Identificar laranjas com base em cor e forma  
- Distinguir ruído (maçãs, folhas, fundo)  
- Contar automaticamente os frutos  
- Determinar métricas geométricas e calibre real  

Este projeto foi desenvolvido no âmbito da unidade curricular de **Visão por Computador** da licenciatura em Engenharia de Sistemas Informáticos.

---

## Features

- Processamento de vídeo em tempo real  
- Segmentação por modelo de cor HSV  
- Limpeza de imagem com operações morfológicas  
- Deteção de objetos (Blob Analysis)  
- Cálculo de métricas (área, perímetro, centroide)  
- Bounding Boxes automáticas  
- Classificação e calibração em milímetros  

---

## Pipeline do Sistema

### 1. Pré-processamento
- Conversão `BGR → RGB → HSV`
- Integração OpenCV com biblioteca IVC

### 2. Segmentação
- Thresholding em HSV
- Remoção de ruído com:
  - Abertura
  - Fecho

### 3. Análise de Blobs
- Labeling de componentes ligados  
- Extração de métricas geométricas  

### 4. Calibração e Output
- Conversão pixel → milímetros  
- Visualização com overlays em tempo real  

---

## Demonstração

O sistema apresenta duas janelas principais:

### Input (BGR)
Visualização original com:
- Bounding boxes  
- Identificação dos objetos  

### Processamento (Máscara Binária)
- Laranjas isoladas em branco  
- Fundo e ruído removidos  

---

## Tech Stack

| Componente | Tecnologia |
|-----------|-----------|
| Linguagem | C++ / C |
| Visão Computacional | OpenCV 4.x |
| Biblioteca de Imagem | IVC |
| Compilador | MinGW-w64 (MSYS2) |

---

## Estrutura do Projeto

```text
C:/VCTP/
├── main.cpp
├── video.avi
└── lib/
    ├── vc.h
    └── vc.c

## Build & Run
### Compilação

g++ -g main.cpp -x c lib/vc.c -x none -o CitrID.exe \
-I C:/msys64/mingw64/include/opencv4 \
-L C:/msys64/mingw64/lib \
-lopencv_core -lopencv_imgproc -lopencv_highgui -lopencv_videoio

## Licença

Projeto desenvolvido para fins académicos - Licenciatura em Engenharia de Sistemas Informáticos (LESI) @ IPCA.

