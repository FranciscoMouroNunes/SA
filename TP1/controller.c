// Sistemas e Automação 2025/26 - TP1
// Controlador "simples" (só estados + switch/case + if)
// Regras do enunciado:
// - OFF: tudo parado, LP=1, CC=0
// - BA (flanco 0->1): passa a OP
// - OP: E1=E2=1, LP=0; LA/LV indicam seleção (BSA/BSV)
// - T1: leva peça até SS, pára, lê SV e amostra MS nesse instante
//       se cor==MS: faz prensagem (MZ) + espera 1.5 s + sobe
//                  depois só envia para a caixa se houver caixa presente e a precisar (<3)
//       senão: rejeita com IP (estende até SIE e recolhe até SIR)
// - T2: leva caixa até SC (SC é lógica negativa: caixa presente quando SC==0), pára,
//       carrega 3 peças, depois anda; CC++ quando a caixa ultrapassa SC (SC 0->1)

#include <stdint.h>
#include <stdbool.h>

#include "IO.c"   // fornece: entradas/saídas + read_inputs/write_outputs/sleep_abs/get_time

#define CYCLE_MS 50

typedef enum { MODE_OFF=0, MODE_OP=1 } ModeState;
typedef enum { BOX_MOVE=0, BOX_LOAD=1 } BoxState;
typedef enum {
  P_SEEK_SS,
  P_wait_for_instruction,     // T1 a andar até SS
  P_PRESS_DOWN,    // MZ=1 até atingir fundo
  P_PRESS_DWELL,   // MZ=1 durante 1.5 s
  P_PRESS_UP,      // MZ=0 até atingir topo
  P_WAIT_BOX,      // espera por caixa pronta
  P_TO_BOX,        // T1 a andar até ST
  P_REJ_EXT,       // IP=1 até SIE
  P_REJ_RET        // IP=0 até SIR
} PieceState;

int main(void) {
  ModeState mode = MODE_OFF;
  BoxState box_st = BOX_MOVE;
  PieceState piece_st = P_SEEK_SS;

  bool prev_BA = false;
  bool prev_SC = true;   // tipicamente sem caixa => SC=1
  bool prev_ST = false;
  bool prev_SS = false;

  uint16_t ms_latch = 0; // 0=BN, 1=Azul, 4=Verde
  uint16_t sv_latch = 0; // 0/1/4
  uint16_t pieces_in_box = 0;

  bool press_moving_seen = false;
  uint64_t dwell_t0 = 0;

  while (1) {
    read_inputs();
	
    // flanco de subida do BA
    bool ba_edge = (!prev_BA && BA);
    prev_BA = BA;

    if (mode == MODE_OFF) {
      // OFF: tudo parado
      E1 = 0; E2 = 0;
      T1 = 0; T2 = 0;
      IP = 0; MZ = 0;
      LA = 0; LV = 0;
      LP = 1;
      CC = 0;

      // reset de estados/variáveis internas
      box_st = BOX_MOVE;
      piece_st = P_SEEK_SS;
      pieces_in_box = 0;
      ms_latch = 0;
      sv_latch = 0;
      press_moving_seen = false;
      dwell_t0 = 0;
      prev_SC = SC;
      prev_ST = ST;
	  prev_SS = SS;

      if (ba_edge) {
        mode = MODE_OP;
      }

    } else { // MODE_OP

      // saídas fixas em operação
      E1 = 1;
      E2 = 1;
      LP = 0;

      // luzes da seleção (estado atual do BS)
      if (BSA) { LA = 1; LV = 0; }
      else if (BSV) { LA = 0; LV = 1; }
      else { LA = 0; LV = 0; }

      // MS atual (0/1/4)
      uint16_t ms_now = 0;
      if (BSA) ms_now = 1;
      else if (BSV) ms_now = 4;
      else ms_now = 0;

      // ---------------- BOX FSM (T2) ----------------
      switch (box_st) {
        case BOX_MOVE:
          T2 = 1;

          // parar quando chega caixa (SC==0, lógica negativa)
          if (SC == 0 && prev_SC == 1) {
            T2 = 0;
            pieces_in_box = 0;
            box_st = BOX_LOAD;
          }
          break;

        case BOX_LOAD:
          T2 = 0;
          if (pieces_in_box >= 3) {
            T2 = 1;
            pieces_in_box = 0;
            CC++;
            box_st = BOX_MOVE;
          }
          break;
      }
      prev_SC = SC;

      bool box_ready = (box_st == BOX_LOAD) && (SC == 0) && (pieces_in_box < 3);

      // ---------------- PIECE FSM (T1 + IP + MZ) ----------------
	  // ainda n funciona direito se nao selecionar nada, mas acho que é favil de arranjar
	  // ver o processo se a peça for selecionada fica parada na ponta
      switch (piece_st) {

        case P_SEEK_SS:
          // leva a peça até SS
          T1 = 1;
          IP = 0;
          MZ = 0;

          if (prev_SS && !SS) {
            // pára e amostra seleção + cor
			
            T1 = 0;
            piece_st = P_wait_for_instruction;
          }
          break;
            /*ms_latch = ms_now;
            sv_latch = SV;

            if ((ms_latch != 0) && (sv_latch == ms_latch)) {
              // match -> prensar
              press_moving_seen = false;
              MZ = 1;
              piece_st = P_PRESS_DOWN;
            } else {
              // mismatch (ou BN) -> rejeitar
              IP = 1;
              piece_st = P_REJ_EXT;
            }
          }
          break;
        */
        
        case P_wait_for_instruction:
          T1 = 0;
          IP = 0;
          MZ = 0;
          if(BSA==0 && BSV==0) {
            // se nada selecionado, volta a procurar por SS
            piece_st = P_wait_for_instruction;
            break;
          } else {
            ms_latch = ms_now;
            sv_latch = SV;

            if ((ms_latch != 0) && (sv_latch == ms_latch)) {
              // match -> prensar
              press_moving_seen = false;
              MZ = 1;
              piece_st = P_PRESS_DOWN;
            } else {
              // mismatch (ou BN) -> rejeitar
              IP = 1;
              piece_st = P_REJ_EXT;
            }
          }
          break;


        case P_PRESS_DOWN:
          T1 = 0;
          IP = 0;
          MZ = 1;

          if (SZ) press_moving_seen = true;               // começou a mover
          if (press_moving_seen && !SZ) {                 // parou no fundo
            dwell_t0 = get_time();
            piece_st = P_PRESS_DWELL;
          }
          break;

        case P_PRESS_DWELL:
          T1 = 0;
          IP = 0;
          MZ = 1;

          if ((get_time() - dwell_t0) >= 1500) {
            press_moving_seen = false;
            MZ = 0;
            piece_st = P_PRESS_UP;
          }
          break;

        case P_PRESS_UP:
          T1 = 0;
          IP = 0;
          MZ = 0;

          if (SZ) press_moving_seen = true;
          if (press_moving_seen && !SZ) {
            piece_st = P_WAIT_BOX;
          }
          break;

        case P_WAIT_BOX:
          T1 = 0;
          IP = 0;
          MZ = 0;

          if (box_ready) {
            // começa a enviar a peça para a caixa
            prev_ST = ST;
            T1 = 1;
            piece_st = P_TO_BOX;
          }
          break;

        case P_TO_BOX:
          T1 = 1;
          IP = 0;
          MZ = 0;

          // conta peça carregada no flanco 0->1 do ST
          if (prev_ST==1 && ST==0) {
            pieces_in_box++;
            piece_st = P_SEEK_SS;
          }
          prev_ST = ST;
          break;

        case P_REJ_EXT:
          T1 = 0;
          MZ = 0;
          IP = 1;

          if (SIE) {
            IP = 0;
            piece_st = P_REJ_RET;
          }
          break;

        case P_REJ_RET:
          T1 = 0;
          MZ = 0;
          IP = 0;

          if (SIR) {
            piece_st = P_SEEK_SS;
          }
          break;
      }
    }
	prev_SS = SS;

    write_outputs();
    sleep_abs(CYCLE_MS);
  }

  return 0;
}
