# CitrID — Citrus Identification & Classification System

![Language](https://img.shields.io/badge/language-C%2B%2B%20%7C%20C-blue)
![OpenCV](https://img.shields.io/badge/OpenCV-4.x-green)
![Platform](https://img.shields.io/badge/platform-Windows-lightgrey)
![Status](https://img.shields.io/badge/status-Academic%20Project-orange)

Este projeto foi desenvolvido para a unidade curricular de **Visão por Computador**. O sistema processa um fluxo de vídeo industrial para identificar, contar e classificar laranjas numa passadeira rolante, distinguindo-as de outros elementos (maçãs e folhas).

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

## Divisão de Trabalho (Branches)

O projeto está dividido em 4 ramos (branches) principais, correspondentes às fases de desenvolvimento:

### 1. `f1-setup-seg` (Estudo do vídeo + segmentação)
* Estudo visual do vídeo e testes nos espaços de cor RGB/HSV.
* Implementação de `vc_rgb_to_hsv_2()` e `vc_hsv_segmentation()`.
* Limpeza morfológica através de operações de `open` e `close`.

### 2. `f2-blobs` (Histograma, equalização e blobs)
* Análise de histogramas e testes de equalização.
* Conversão de máscara (3 canais para 1 canal).
* Etiquetagem e extração de dados via `vc_binary_blob_labelling()` e `vc_binary_blob_info()`.

### 3. `f3-calibre` (Geometria, calibre e categoria)
* Cálculo de métricas: circularidade e diâmetro equivalente.
* Conversão de unidades (px -> mm) e tabela de calibres por categoria.
* Testes de deteção de arestas (Sobel/Prewitt) se necessário.

### 4. `f4-integracao` (Integração final e tracking)
* Consolidação do pipeline no `main.cpp`.
* Implementação de lógica de contagem na frame e tracking para total acumulado.
* Overlay gráfico com OpenCV e realização de testes finais.

---

## Estrutura do Projeto

```text
C:/VCTP/
├── main.cpp          # Integrador do Pipeline
├── video.avi         # Ficheiro de vídeo (Ignore via .gitignore)
├── docs/             # Manuais e documentação técnica
│   └── Manual_Configuracao_Final_VCTP.html
└── lib/              # Biblioteca de Visão por Computador (C)
    ├── vc.h          
    └── vc.c
```

## Build & Run
### Compilação

g++ -g main.cpp -x c lib/vc.c -x none -o CitrID.exe \
-I C:/msys64/mingw64/include/opencv4 \
-L C:/msys64/mingw64/lib \
-lopencv_core -lopencv_imgproc -lopencv_highgui -lopencv_videoio

## Licença

Projeto desenvolvido para fins académicos - Licenciatura em Engenharia de Sistemas Informáticos (LESI) @ IPCA.



