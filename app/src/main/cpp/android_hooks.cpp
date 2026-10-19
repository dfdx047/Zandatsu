// ProjectSwordMan - hooks especificos do Android.
//
// Vazio por agora. E aqui que entram, por exemplo:
//
//  - a espera do D3D do Xbox 360 por espaco no ring buffer da GPU. No 360 o loop
//    usa "cctpl / db16cyc / cctpm" (baixa a prioridade da thread e pausa uns
//    ciclos); recompilado fica vazio e a thread principal gira a 100% de um
//    nucleo. No DBZ3-Android era sub_82235918 (66% da thread do jogo no
//    Snapdragon 8 Elite). No MGR o endereco e outro: encontra-lo com
//    "optxbox d3dmap --generated generated/default" e depois:
//
//      #include <rex/hook.h>
//      REX_EXTERN(__imp__sub_XXXXXXXX);
//      REX_HOOK_RAW(sub_XXXXXXXX) {
//        std::this_thread::yield();
//        __imp__sub_XXXXXXXX(ctx, base);
//      }
//
//  - a pausa do jogo por baixo dos menus do app.
//
// Atencao: um REX_HOOK_RAW de um sub_ que nao existe no codigo gerado do MGR da
// erro de link (o __imp__ nao existe). Usar so enderecos do generated/default.
