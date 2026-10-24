# LPRO SILVER – Limitador True Peak (VST3)

Plugin de limiter em C++/JUCE, inspirado nas funções de um limitador true peak profissional. DSP e interface são originais.

## Como compilar no Windows

1. Instale: **Visual Studio 2022** (carga de trabalho "Desenvolvimento para desktop com C++"), **CMake 3.22+** e **Git**.
2. Abra o "Developer PowerShell for VS 2022" na pasta `LPROSilver` e rode:

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

O primeiro build baixa o JUCE (precisa de internet) e demora alguns minutos.

3. O plugin fica em:
   `build\LPROSilver_artefacts\Release\VST3\LPRO SILVER.vst3`
4. Copie a pasta `LPRO SILVER.vst3` para `C:\Program Files\Common Files\VST3\` (peça permissão de administrador).

## Usar no REAPER

1. `Options > Preferences > Plug-ins > VST`: confirme que o caminho `C:\Program Files\Common Files\VST3` está listado.
2. Clique em **Re-scan** (ou "Clear cache and re-scan").
3. No master ou numa faixa: `FX > Add > VST3: LPRO SILVER (Silver Audio)`.
4. Se mexer em Lookahead ou Oversampling, o plugin informa a latência ao REAPER (compensação automática).

## Parâmetros

| Parâmetro | Faixa | Função |
|---|---|---|
| Gain | 0 a +30 dB | Ganho de entrada antes do limiter |
| Output Ceiling | -30 a 0 dB | Teto de saída |
| Lookahead | 0 a 5 ms | Antecipação (janela de detecção) |
| Attack | 0 a 100 % | Fração do lookahead usada para suavizar a descida do ganho |
| Release | 10 a 2000 ms | Tempo de recuperação |
| Style | 4 opções | Multiplica o release (Transparent mais lento, Aggressive mais rápido) |
| Oversampling | Off a 16x | Filtro FIR de fase linear |
| Link Transients / Link Release | 0 a 100 % | Quanto os canais compartilham a redução |
| True Peak | on/off | Quando ligado, força no mínimo 4x de oversampling |
| Dither | Off / 16 / 24 bit | Dither TPDF na saída |

## Checklist de revisão (vamos marcando juntos)

**Compilação e carregamento**
- [ ] Compila sem erros no VS 2022
- [ ] REAPER encontra e carrega o VST3
- [ ] Estado salva/recarrega com o projeto

**Limitação**
- [ ] Sinal a 0 dBFS com Gain +12 dB nunca passa do Ceiling (conferir no medidor do REAPER)
- [ ] Sem cliques ao mudar Lookahead, Oversampling ou True Peak
- [ ] Latência reportada confere com o atraso real (teste com impulso)
- [ ] Release e Style soam como esperado em material musical
- [ ] Link Transients/Release: imagem estéreo estável com 100 %

**Interface**
- [ ] Display de forma de onda mostra entrada, saída e GR em vermelho
- [ ] Medidor de saída e leituras LUFS respondem
- [ ] Todos os controles funcionam e respondem à automação do REAPER

**Medição**
- [ ] LUFS conferem com um medidor de referência (ex.: JS: Loudness Meter do REAPER)

## Limitações da v0.1 (o que ainda não é igual a um limiter comercial)

- Os "Styles" mudam só o tempo de release; ainda não há algoritmos distintos com release adaptativo.
- O true peak usa oversampling mínimo de 4x e clamp final; não há medição ITU separada de 4x no medidor.
- Sem side-chain externo, sem surround, sem A/B nem undo/redo, sem rótulos de pico de GR no display.
- Sem MIDI Learn, mas todos os parâmetros são automatizáveis no REAPER.

## Estrutura

- `Source/Limiter.h`: núcleo DSP (lookahead, mínimo deslizante, release, linking)
- `Source/LoudnessMeter.h`: LUFS (K-weighting, gates)
- `Source/PluginProcessor.*`: parâmetros, oversampling, medição
- `Source/PluginEditor.*`: interface
