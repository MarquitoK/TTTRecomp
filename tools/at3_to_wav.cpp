// Convierte corrientes ATRAC3 crudas de TTT a wav con el mismo decodificador del runtime.
// Uso: at3_a_wav <entrada.unk> <salida.wav> [musica|voz]
// Sin tercer argumento se deduce por el tamano: si calza en tramas de 304 es musica.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "audio/atrac3.h"

#define TRAMA_MONO 152
#define TRAMA_EST 304

static void escribir_u32(FILE* f, uint32_t v) { fwrite(&v, 4, 1, f); }
static void escribir_u16(FILE* f, uint16_t v) { fwrite(&v, 2, 1, f); }

int main(int argc, char** argv) {
    if (argc < 3) { printf("uso: at3_a_wav <entrada> <salida.wav> [musica|voz]\n"); return 2; }
    FILE* e = fopen(argv[1], "rb");
    if (!e) { printf("no puedo abrir %s\n", argv[1]); return 1; }
    fseek(e, 0, SEEK_END);
    long n = ftell(e);
    fseek(e, 0, SEEK_SET);
    std::vector<uint8_t> d((size_t)n);
    if (fread(d.data(), 1, (size_t)n, e) != (size_t)n) { fclose(e); printf("lectura corta\n"); return 1; }
    fclose(e);

    bool musica = argc > 3 ? !strcmp(argv[3], "musica") : (n - 1) % TRAMA_EST == 0;
    int trama = musica ? TRAMA_EST : TRAMA_MONO;
    int canales = musica ? 2 : 1;
    int rate = musica ? 44100 : 22050;
    long tramas = n / trama;
    if (tramas < 1) { printf("%s no tiene tramas completas\n", argv[1]); return 1; }

    At3 dec;
    int conjunto = 0;
    if (canales == 2) {
        At3 prueba;
        if (!at3_iniciar(prueba, 2, trama, 0) && at3_decodificar(prueba, d.data())) conjunto = 1;
    }
    if (at3_iniciar(dec, canales, trama, conjunto) || !dec.listo) { printf("no puedo iniciar el decodificador\n"); return 1; }

    FILE* s = fopen(argv[2], "wb");
    if (!s) { printf("no puedo escribir %s\n", argv[2]); return 1; }
    uint32_t datos = (uint32_t)(tramas * AT3_MUESTRAS * canales * 2);
    fwrite("RIFF", 1, 4, s); escribir_u32(s, 36 + datos); fwrite("WAVE", 1, 4, s);
    fwrite("fmt ", 1, 4, s); escribir_u32(s, 16);
    escribir_u16(s, 1); escribir_u16(s, (uint16_t)canales); escribir_u32(s, (uint32_t)rate);
    escribir_u32(s, (uint32_t)(rate * canales * 2)); escribir_u16(s, (uint16_t)(canales * 2)); escribir_u16(s, 16);
    fwrite("data", 1, 4, s); escribir_u32(s, datos);

    std::vector<int16_t> pcm((size_t)AT3_MUESTRAS * canales);
    long errores = 0;
    for (long k = 0; k < tramas; k++) {
        if (at3_decodificar(dec, d.data() + (size_t)k * trama)) { errores++; memset(pcm.data(), 0, pcm.size() * 2); }
        else
            for (int i = 0; i < AT3_MUESTRAS; i++)
                for (int c = 0; c < canales; c++) {
                    float v = dec.sal[c < dec.canales ? c : dec.canales - 1][i] * 32767.0f;
                    pcm[(size_t)i * canales + c] = (int16_t)(v > 32767.0f ? 32767 : v < -32768.0f ? -32768 : (int)v);
                }
        fwrite(pcm.data(), 2, pcm.size(), s);
    }
    fclose(s);
    if (errores) printf("%s: %ld tramas con error de %ld\n", argv[1], errores, tramas);
    return errores == tramas ? 1 : 0;
}
