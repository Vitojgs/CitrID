# Trabalho Prático — Visão por Computador
## Deteção e Classificação de Laranjas em Linha de Produção
### Instituto Politécnico do Cávado e do Ave — 2025/2026

---

## Índice

1. [Contexto e Restrições](#1-contexto-e-restrições)
2. [Versão 1 — min.cpp (base)](#2-versão-1--mincpp-base)
3. [Versão 2 — main.cpp (melhorada)](#3-versão-2--maincpp-melhorada)
4. [Versão 3 — main.cpp (final)](#4-versão-3--maincpp-final)
5. [Tabela Comparativa das Três Versões](#5-tabela-comparativa-das-três-versões)

---

## 1. Contexto e Restrições

O projeto processa o ficheiro `video.avi` frame a frame para detetar laranjas numa linha de produção, calcular as suas métricas geométricas e classificá-las por calibre segundo o **Regulamento CEE nº 379/71**.

### Restrições impostas pelo enunciado

- Todo o processamento de imagem deve usar a biblioteca própria `vc.c` / `vc.h` com as estruturas `IVC` e `OVC`.
- O código deve seguir o padrão do `CodigoExemplo.cpp` fornecido pelo professor.
- É permitido usar **no máximo 3 funções ou instâncias de classes OpenCV extra** para além das já presentes no `CodigoExemplo.cpp`.

### Funções OpenCV já presentes no CodigoExemplo.cpp (não contam para o limite)

| Função / Classe | Utilização |
|---|---|
| `cv::VideoCapture` | Leitura do ficheiro de vídeo |
| `cv::Mat` | Representação da frame OpenCV |
| `cv::namedWindow()` | Criação de janelas de visualização |
| `cv::imshow()` | Exibição de frames nas janelas |
| `cv::waitKey()` | Controlo de pausa e leitura de tecla |
| `cv::putText()` | Escrita de texto sobre a imagem |
| `cv::destroyWindow()` | Fecho de janelas |
| `cv::Point()` | Coordenada 2D para posicionamento |
| `cv::Scalar()` | Cor em formato BGR |
| `cv::CAP_PROP_*` | Propriedades do vídeo (FPS, dimensões, etc.) |
| `cv::FONT_HERSHEY_SIMPLEX` | Fonte de texto |
| `cv::WINDOW_AUTOSIZE` | Modo da janela |

### Calibração de escala (comum a todas as versões)

O enunciado estabelece que **280 px = 55 mm**, o que permite converter medidas entre pixéis e milímetros:

```
MM_POR_PIXEL  = 55.0 / 280.0  ≈ 0.196 mm/px
PIXELS_POR_MM = 280.0 / 55.0  ≈ 5.091 px/mm
```

Esta relação é usada para:
- Estimar o **diâmetro** de cada laranja em mm (a partir da bounding box em px)
- Converter a **área** de px² para mm²
- Converter o **perímetro** de px para mm
- Classificar cada laranja por **calibre** segundo o Regulamento CEE nº 379/71

### Tabela de calibres — Regulamento CEE nº 379/71

| Calibre | Diâmetro mínimo (mm) |
|---|---|
| CAL 0 | ≥ 100 |
| CAL 1 | ≥ 87 |
| CAL 2 | ≥ 84 |
| CAL 3 | ≥ 81 |
| CAL 4 | ≥ 77 |
| CAL 5 | ≥ 73 |
| CAL 6 | ≥ 70 |
| CAL 7 | ≥ 67 |
| CAL 8 | ≥ 64 |
| CAL 9 | ≥ 62 |
| CAL 10 | ≥ 60 |
| CAL 11 | ≥ 58 |
| CAL 12 | ≥ 56 |
| CAL 13 | ≥ 53 |
| < MIN | < 53 |

---

## 2. Versão 1 — min.cpp (base)

### Objetivo

Implementação mínima funcional: detetar laranjas, calcular as suas métricas e classificá-las por calibre, seguindo rigorosamente as restrições do enunciado.

### Pipeline de processamento

```
Frame BGR (OpenCV)
        │
        ▼  bgr_to_ivc_rgb()          [função auxiliar própria]
Frame RGB → IVC (3 canais, R-G-B)
        │
        ▼  vc_rgb_to_hsv()           [vc.c]
Imagem HSV
        │
        ▼  vc_hsv_segmentation()     [vc.c]
           H: 10°–40°  S: 90–255  V: 80–255
Binário: branco = pixels cor laranja
        │
        ▼  vc_binary_open(kernel=5)  [vc.c]  → remove ruído pequeno
        ▼  vc_binary_close(kernel=9) [vc.c]  → preenche buracos internos
Binário limpo
        │
        ▼  vc_binary_blob_labelling()  [vc.c]
        ▼  vc_binary_blob_info()       [vc.c]
Blobs com: área, perímetro, bounding box, centróide
        │
        ▼  Filtro: área ≥ 8 000 px²
Laranjas válidas
        │
        ▼  Cálculo: diâmetro mm, área mm², perímetro mm
        ▼  classificar_laranja()      [função própria]
        ▼  Anotação: cv::rectangle + cv::circle + cv::putText
Frame anotada → cv::imshow
```

### Funções da lib vc.c utilizadas

| Função | Porquê é usada |
|---|---|
| `vc_image_new()` | Aloca 5 imagens IVC reutilizadas em cada frame (rgb, hsv, seg, tmp, lbl), evitando alocações repetidas no loop |
| `vc_image_free()` | Liberta a memória alocada no fim do programa |
| `vc_rgb_to_hsv()` | Converte a imagem RGB para o espaço HSV, onde a cor laranja é muito mais fácil de segmentar do que em RGB (o Hue isola a tonalidade independentemente do brilho) |
| `vc_hsv_segmentation()` | Gera uma imagem binária onde os pixels dentro do intervalo H:10°–40°, S:90–255, V:80–255 ficam a branco — corresponde à gama de cor das laranjas |
| `vc_binary_open()` | Erosão seguida de dilatação com kernel=5; remove regiões brancas pequenas (ruído, reflexos) que não são laranjas |
| `vc_binary_close()` | Dilatação seguida de erosão com kernel=9; preenche os buracos escuros no interior das laranjas causados por reflexos de luz |
| `vc_binary_blob_labelling()` | Rotula cada região conexa branca com um identificador único; retorna um array `OVC` com uma entrada por blob |
| `vc_binary_blob_info()` | Calcula para cada blob: área (px), perímetro (px), bounding box (x, y, width, height) e centro de massa (xc, yc) |

### Funções OpenCV extras usadas (2 de 3 permitidas)

| Função | Para que serve |
|---|---|
| `cv::rectangle()` | Desenha a bounding box de cada laranja e o painel HUD de fundo preto |
| `cv::circle()` | Marca o centro de massa de cada laranja com um ponto vermelho |

### Função auxiliar própria

**`bgr_to_ivc_rgb()`** — O OpenCV entrega os frames em formato BGR (Blue-Green-Red), mas `vc_rgb_to_hsv()` espera os canais na ordem R, G, B. Esta função percorre todos os pixels e inverte os canais B e R ao copiar para o IVC, sem recorrer a nenhuma função OpenCV extra.

### Informação exibida

**Painel HUD** (canto superior esquerdo, fundo preto):
- Resolução do vídeo
- Número da frame atual / total de frames
- Número de laranjas detetadas na frame atual
- Total acumulado de laranjas (soma de todas as frames)
- Escala de calibração (280px = 55mm)

**Por laranja** (sobre cada blob válido):
- Bounding box laranja
- Ponto vermelho no centróide
- Etiqueta com o calibre CEE (ex: `CAL 5 (73-84mm)`)
- Etiqueta com o diâmetro estimado em mm (ex: `D:78mm`)

**Janela secundária:** imagem binária da segmentação HSV.

**Terminal:** log por frame com área, perímetro, diâmetro, calibre e coordenadas do centróide de cada laranja.

### Limitação desta versão

A contagem total acumulada soma todas as deteções frame a frame, **contando a mesma laranja múltiplas vezes** (uma vez por cada frame em que aparece). Esta limitação é resolvida na Versão 2.

---

## 3. Versão 2 — main.cpp (melhorada)

### O que é acrescentado face à Versão 1

A Versão 2 introduz três melhorias sobre a base da Versão 1:

1. **Equalização do histograma do canal V** — torna a segmentação robusta à iluminação
2. **Circularidade** — filtra blobs irregulares (laranjas sobrepostas ou ruído)
3. **Tracker de centróides** — resolve o problema da dupla contagem

### Pipeline de processamento

```
Frame BGR (OpenCV)
        │
        ▼  bgr_to_ivc_rgb()
Frame RGB → IVC
        │
        ▼  vc_rgb_to_hsv()
Imagem HSV
        │
        ▼  extrair_canal_v()              [função auxiliar própria]
Canal V (grayscale, 1 canal)
        │
        ▼  vc_gray_histogram_equalization() [vc.c]  ← NOVO
Canal V equalizado
        │
        ▼  vc_gray_lowpass_gaussian_filter() [vc.c] ← NOVO
Canal V suavizado
        │
        ▼  repor_canal_v()                [função auxiliar própria]
Imagem HSV com canal V normalizado
        │
        ▼  vc_hsv_segmentation()
           H: 10°–40°  S: 80–255  V: 60–255  (limiares V mais baixos após equalização)
Binário
        │
        ▼  vc_binary_open(kernel=5)
        ▼  vc_binary_close(kernel=9)
Binário limpo
        │
        ▼  vc_binary_blob_labelling()
        ▼  vc_binary_blob_info()
Blobs com métricas
        │
        ▼  Filtro: área ≥ 20 000 px²
        ▼  calcular_circularidade()       [cálculo próprio]  ← NOVO
        ▼  Filtro qualidade: circ ≥ 0.55 (caso contrário → IRREGULAR)
        │
        ▼  tracker_associar()             [função auxiliar própria]  ← NOVO
        ▼  Se nova → total_laranjas++
        │
        ▼  Anotação: cv::rectangle + cv::circle + cv::putText
Frame anotada → cv::imshow
```

### Funções da lib vc.c adicionadas face à Versão 1

| Função | Porquê é usada |
|---|---|
| `vc_gray_histogram_equalization()` | Recebe o canal V isolado (grayscale) e redistribui os valores de brilho para ocupar toda a gama [0,255]. Isto normaliza variações de iluminação do ambiente (sombras nas laranjas, reflexos da passadeira, diferenças entre frames) tornando a segmentação HSV muito mais estável |
| `vc_gray_lowpass_gaussian_filter()` | Aplica um kernel Gaussiano 5×5 ao canal V já equalizado, suavizando ruído de compressão do vídeo antes de repor o canal no HSV. Evita que artefactos de compressão gerem falsos positivos na segmentação |

### Funções auxiliares próprias adicionadas

**`extrair_canal_v()`** — Copia apenas o terceiro canal (índice 2, correspondente ao V) da imagem HSV de 3 canais para um IVC de 1 canal grayscale. Necessária porque `vc_gray_histogram_equalization()` e `vc_gray_lowpass_gaussian_filter()` trabalham exclusivamente com imagens de 1 canal.

**`repor_canal_v()`** — Faz o inverso: copia o canal V já processado (equalizado e suavizado) de volta para o índice 2 da imagem HSV, preservando os canais H e S intactos.

**`calcular_circularidade()`** — Implementa a fórmula:

```
circularidade = 4 × π × área / perímetro²
```

Para um círculo perfeito, o valor é 1.0. Para uma laranja real, fica tipicamente entre 0.65 e 0.90. Valores abaixo de 0.55 indicam um blob alongado ou irregular — duas laranjas coladas que o labelling tratou como uma só, ou um objeto estranho que passou no segmentador. Estes blobs são marcados como `IRREGULAR` em vez de receberem uma classificação de calibre.

**`tracker_ja_existe()`** — Para cada novo blob válido, calcula a distância euclidiana entre o seu centróide e o de cada entrada do tracker. Se a distância mínima for ≤ 80px, o blob é associado à mesma laranja e não incrementa a contagem. Caso contrário, é uma laranja nova e é adicionada ao tracker. Entradas ausentes há mais de 8 frames consecutivas são removidas.

### Funções OpenCV extras usadas (2 de 3 permitidas — igual à Versão 1)

`cv::rectangle()` e `cv::circle()` — as mesmas da Versão 1. A Versão 2 não adiciona nenhuma função OpenCV extra; as melhorias são todas feitas com funções da lib `vc.c` ou com lógica C++ pura.

### Informação exibida (adições face à Versão 1)

**Painel HUD** — o total acumulado passa a ser de **laranjas únicas** (via tracker), eliminando a dupla contagem. É adicionada uma linha de legenda `laranja=ok / vermelho=irregular`.

**Por laranja:**
- A **bounding box** fica **laranja** para laranjas normais e **vermelha** para blobs irregulares
- A etiqueta de calibre é substituída por `IRREGULAR` quando a circularidade é baixa
- É acrescentada uma etiqueta de **circularidade** (ex: `C:82%`) sob o diâmetro

---

## 4. Versão 3 — main.cpp (final)

### O que é acrescentado face à Versão 2

A Versão 3 introduz uma única adição, mas visualmente muito impactante:

- **Rastro de trajetória** com `cv::line()` — a 3ª e última função OpenCV extra permitida

Para suportar o rastro, o `TrackerEntry` é expandido com um `id` único por laranja e um histórico de centróides (`vector<Ponto> rastro`), e a função `tracker_ja_existe()` é substituída por `tracker_associar()` que gere o histórico.

### Pipeline de processamento

O pipeline de processamento de imagem é **idêntico ao da Versão 2**. A única diferença está na fase de anotação, onde é adicionado um passo antes das bounding boxes:

```
...
Blobs com métricas (igual à Versão 2)
        │
        ▼  tracker_associar()        → actualiza historico do rastro
        │
        ▼  desenhar_rastro()         ← NOVO (cv::line)
           Para cada entrada activa no tracker:
           percorre o vector rastro e liga posições
           consecutivas com cv::line(), com espessura
           e brilho crescentes do mais antigo para o mais recente
        │
        ▼  cv::rectangle + cv::circle + cv::putText
           (bounding box na cor do rastro da laranja)
Frame anotada → cv::imshow
```

### Estrutura TrackerEntry expandida

```cpp
struct TrackerEntry {
    int xc, yc;                 // centroide na ultima frame vista
    int frames_ausente;         // frames sem correspondencia
    int id;                     // identificador unico por laranja  ← NOVO
    std::vector<Ponto> rastro;  // historico de centroides          ← NOVO
};
```

O campo `id` é atribuído sequencialmente (`proximo_id++`) cada vez que uma laranja nova entra no tracker. É usado para indexar a paleta de cores `CORES_RASTRO[]`, garantindo que cada laranja tem sempre a mesma cor ao longo de toda a sua passagem no vídeo.

### Função `desenhar_rastro()`

```
Para i = 1 até n-1 (posições do historico):
    fator = i / (n - 1)         → 0.0 (mais antigo) a 1.0 (mais recente)
    espessura = 1 + fator × 2   → 1px a 3px
    cor = cor_base × fator      → escurece para posições antigas
    cv::line(rastro[i-1], rastro[i], cor, espessura)
```

O efeito de desvanecimento é puramente matemático: a cor base da laranja é multiplicada pelo factor `fator`, tornando os segmentos mais antigos mais escuros e finos. Não são usadas imagens auxiliares nem transparência — apenas aritmética sobre os valores BGR da `cv::Scalar`.

O histórico guarda no máximo `RASTRO_MAX_POS = 20` posições. A 25 FPS, isto corresponde a aproximadamente **0.8 segundos de trajetória visível**. Quando o limite é atingido, a posição mais antiga é removida (FIFO).

### Paleta de cores

São definidas 8 cores distintas em BGR para os rastros e bounding boxes:

| Índice | Cor |
|---|---|
| 0 | Amarelo-torrado |
| 1 | Azul-claro |
| 2 | Verde-limão |
| 3 | Magenta |
| 4 | Ciano-claro |
| 5 | Vermelho-vivo |
| 6 | Verde-amarelado |
| 7 | Rosa |

Se houver mais de 8 laranjas em simultâneo, as cores repetem-se ciclicamente (`id % 8`).

### Funções OpenCV extras usadas (3 de 3 — limite esgotado)

| Função | Para que serve |
|---|---|
| `cv::rectangle()` | Bounding box e painel HUD (igual às versões anteriores) |
| `cv::circle()` | Centro de massa de cada laranja (igual às versões anteriores) |
| `cv::line()` | Segmentos do rastro de trajetória entre posições consecutivas do histórico |

### Informação exibida (adições face à Versão 2)

**Por laranja:**
- **Rastro de trajetória** desenhado por baixo de todas as outras anotações, composto por segmentos de linha que desvanecem do centro de massa atual para trás
- A **bounding box** tem agora a **mesma cor do rastro** da laranja, tornando imediata a associação visual entre as duas

**Painel HUD** — é acrescentada uma linha informativa com o número de posições do rastro e a duração temporal correspondente (ex: `RASTRO: ultimas 20 posicoes (~0s de trajeto)`).

---

## 5. Tabela Comparativa das Três Versões

| Característica | Versão 1 (min.cpp) | Versão 2 (main.cpp melhorada) | Versão 3 (main.cpp final) |
|---|---|---|---|
| **Funções vc.c** | 8 | 10 | 10 |
| **Extras OpenCV** | 2 / 3 | 2 / 3 | 3 / 3 |
| **Segmentação HSV** | Direta sobre RGB→HSV | Com equalização V + Gaussian | Igual à V2 |
| **Robustez à iluminação** | Baixa | Alta | Alta |
| **Análise de forma** | Área + perímetro | + Circularidade | + Circularidade |
| **Deteção de irregulares** | Não | Sim (circ < 0.55) | Sim (circ < 0.55) |
| **Contagem acumulada** | Soma frame a frame (duplica laranjas) | Laranjas únicas via tracker | Laranjas únicas via tracker |
| **Rastro de trajetória** | Não | Não | Sim (cv::line, 20 pos.) |
| **Cor por laranja** | Todas iguais | Box vermelha se irregular | Cor única por laranja |
| **Janela binária** | Sim | Sim | Sim |
| **Log no terminal** | Sim | Sim | Sim (com ID da laranja) |

### Funções vc.c em cada versão

| Função | V1 | V2 | V3 |
|---|---|---|---|
| `vc_image_new()` | ✓ | ✓ | ✓ |
| `vc_image_free()` | ✓ | ✓ | ✓ |
| `vc_rgb_to_hsv()` | ✓ | ✓ | ✓ |
| `vc_hsv_segmentation()` | ✓ | ✓ | ✓ |
| `vc_binary_open()` | ✓ | ✓ | ✓ |
| `vc_binary_close()` | ✓ | ✓ | ✓ |
| `vc_binary_blob_labelling()` | ✓ | ✓ | ✓ |
| `vc_binary_blob_info()` | ✓ | ✓ | ✓ |
| `vc_gray_histogram_equalization()` | — | ✓ | ✓ |
| `vc_gray_lowpass_gaussian_filter()` | — | ✓ | ✓ |

### Funções OpenCV extra em cada versão

| Função | V1 | V2 | V3 |
|---|---|---|---|
| `cv::rectangle()` | ✓ | ✓ | ✓ |
| `cv::circle()` | ✓ | ✓ | ✓ |
| `cv::line()` | — | — | ✓ |
