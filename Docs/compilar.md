# Compilar e Executar o Projeto VCTP

## Instituto Politécnico do Cávado e do Ave - 2025/2026

---

## Requisitos

- **OpenCV 4** instalado (`libopencv-dev`)
- **g++** (compilador C++)
- **Docker** (opcional)

---

## Compilação Direta (Linux)

### Comando único:
```bash
cd /home/ugrt/Downloads/VCTP
g++ -o trabpratico TrabPratico/main.cpp lib/vc.c -I lib $(pkg-config --cflags opencv4) $(pkg-config --libs opencv4) -lm
```

### Compilar e executar:
```bash
cd /home/ugrt/Downloads/VCTP
g++ -o trabpratico TrabPratico/main.cpp lib/vc.c -I lib $(pkg-config --cflags opencv4) $(pkg-config --libs opencv4) -lm && ./trabpratico
```

---

## Compilação com Docker

### Construir a imagem:
```bash
cd /home/ugrt/Downloads/VCTP
docker build -t vctp .
```

### Executar o contentor:
```bash
docker run -it --rm -v $(pwd):/project vctp
```

---

## Explicação dos parâmetros

| Parâmetro | Descrição |
|-----------|-----------|
| `-o trabpratico` | Nome do ficheiro executável de saída |
| `TrabPratico/main.cpp` | Ficheiro principal C++ |
| `lib/vc.c` | Biblioteca C de processamento de imagem |
| `-I lib` | Diretório de inclui os headers (`vc.h`) |
| `$(pkg-config --cflags opencv4)` | Flags de compilação do OpenCV |
| `$(pkg-config --libs opencv4)` | Bibliotecas de linkage do OpenCV |
| `-lm` | Liga a biblioteca matemática |

---

## VSCode

Para compilar e executar no VSCode:

1. Abre o projeto no VSCode
2. Pressiona `Ctrl+Shift+B` e seleciona **"Build and Run"**
3. Ou usa `F5` para compilar e fazer debug

---

## Makefile

O projeto inclui um ficheiro `Makefile` na raiz que simplifica a compilação e execução.

### Comandos disponíveis:

| Comando | Descrição |
|---------|-----------|
| `make` | Compilar o projeto |
| `make clean` | Remover o executável |
| `make run` | Compilar e executar |
| `make docker-build` | Criar imagem Docker |
| `make docker-run` | Criar e executar contentor Docker |
| `make help` | Mostrar ajuda |

### Exemplos de uso:

```bash
# Compilar
cd /home/ugrt/Downloads/VCTP
make

# Compilar e executar
make run

# Limpar executável
make clean

# Com Docker
make docker-run
```

---

## Notas

- O vídeo de entrada deve chamar-se `video.avi` e estar na pasta `TrabPratico/`
- O programa abre uma janela com o vídeo processado
- Pressiona `q` para sair