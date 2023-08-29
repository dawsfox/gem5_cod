#include "codelet/SCMUlate/include/modules/fetch_decode.hpp"
#include "codelet/su.hh"
#include <string>
#include <vector>

scm::fetch_decode_module::fetch_decode_module(inst_mem_module *const inst_mem, 
                                              control_store_module *const control_store_m, 
                                              bool *const aliveSig, 
                                              ILP_MODES ilp_mode,
                                              gem5::SU * owner,
                                              uint64_t root) : 
                                              inst_mem_m(inst_mem),
                                              ctrl_st_m(control_store_m),
                                              aliveSignal(aliveSig),
                                              PC(0),
                                              su_number(0), 
                                              instructionLevelParallelism(ilp_mode, this, root), 
                                              owner(owner),
                                              stallingInstruction(nullptr)
                                              //debugger(DEBUGER_MODE)
                                              
{
}

int scm::fetch_decode_module::behavior()
{
  ITT_DOMAIN(fetch_decode_module_behavior);
  ITT_STR_HANDLE(checkMarkInstructionToSched);
  ITT_STR_HANDLE(instructionFinished);
  uint64_t stall = 0, waiting = 0, ready = 0, execution_done = 0, executing = 0, decomision = 0;
  bool commited = false; 
  TIMERS_COUNTERS_GUARD(
      this->time_cnt_m->addEvent(this->su_timer_name, SU_START););
  SCMULATE_INFOMSG(1, "Initializing the SU");
// Initialization barrier
//#pragma omp barrier
  while (*(this->aliveSignal)) {
    // FETCHING PC
    int fetch_reps = 0;
    scm::decoded_instruction_t *new_inst = nullptr;
    if (!commited) {
      do {
        if (this->stallingInstruction == nullptr) {
          SCMULATE_INFOMSG(5, "FETCHING PC = %d", this->PC);
          new_inst = this->inst_mem_m->fetch(this->PC);
          if (!new_inst) {
            *(this->aliveSignal) = false;
            SCMULATE_ERROR(0, "Returned instruction is NULL for PC = %d. This should not happen", PC);
            continue;
          }
          // Insert new instruction
          if (this->inst_buff_m.add_instruction(*new_inst)) {
            TIMERS_COUNTERS_GUARD(
              this->time_cnt_m->addEvent(this->su_timer_name, FETCH_DECODE_INSTRUCTION, std::string("PC = ") + std::to_string(PC) + std::string(" ") + new_inst->getFullInstruction()););
            SCMULATE_INFOMSG(5, "Executing PC = %d", this->PC);
            ITT_TASK_BEGIN(fetch_decode_module_behavior, checkMarkInstructionToSched);
            instructionLevelParallelism.checkMarkInstructionToSched(this->inst_buff_m.get_latest());
            ITT_TASK_END(checkMarkInstructionToSched);
            if (this->inst_buff_m.get_latest()->second == instruction_state::STALL) {
                this->stallingInstruction = this->inst_buff_m.get_latest();
                SCMULATE_INFOMSG(5, "Stalling on %s", stallingInstruction->first->getFullInstruction().c_str());
            }

            // Mark instruction for scheduling
            commited = new_inst->getOpcode() == COMMIT_INST.opcode;
            SCMULATE_INFOMSG(5, "incrementing PC");
            this->PC++;
            TIMERS_COUNTERS_GUARD(
              this->time_cnt_m->addEvent(this->su_timer_name, SU_IDLE, std::string("PC = ") + std::to_string(PC)) );
          }
        } else {
          new_inst = this->stallingInstruction->first;
          if (this->stallingInstruction->second != instruction_state::STALL) {
            SCMULATE_INFOMSG(5, "Unstalling on %s", stallingInstruction->first->getFullInstruction().c_str());
            this->stallingInstruction = nullptr;
          }
        }
      } while (stallingInstruction == nullptr && !commited && ++fetch_reps < INSTRUCTION_FETCH_WINDOW && new_inst->getType() != instType::CONTROL_INST && new_inst->getType() != instType::COMMIT );
    }
    // bool mark_event = (this->inst_buff_m.getBufferSize() > 0 );
    // if (mark_event) {
    //   TIMERS_COUNTERS_GUARD(
    //       this->time_cnt_m->addEvent(this->su_timer_name, DISPATCH_INSTRUCTION, std::string("PC = ") + std::to_string(PC) + std::string(" ") + new_inst->getFullInstruction()););
    // }
    // Stats to collect
    stall = 0; waiting = 0; ready = 0; execution_done = 0; executing = 0; decomision = 0;
    // Iterate over the instruction buffer (window) looking for instructions to execute
    for (auto it = this->inst_buff_m.get_buffer()->begin(); it != this->inst_buff_m.get_buffer()->end(); ++it) {
      //#pragma omp flush acquire
      instruction_state_pair * current_pair = *it;
      switch (current_pair->second) {
        case instruction_state::STALL:
          stall++;
          ITT_TASK_BEGIN(fetch_decode_module_behavior, checkMarkInstructionToSched);
          instructionLevelParallelism.checkMarkInstructionToSched(current_pair);
          ITT_TASK_END(checkMarkInstructionToSched);
          if (current_pair->second == instruction_state::STALL)
            this->stallingInstruction = current_pair;
          break;
        case instruction_state::WAITING:
          waiting++;
          // TIMERS_COUNTERS_GUARD(
          //   this->time_cnt_m->addEvent(this->su_timer_name, FETCH_DECODE_INSTRUCTION, current_pair->first->getFullInstruction()););
          ITT_TASK_BEGIN(fetch_decode_module_behavior, checkMarkInstructionToSched);
          instructionLevelParallelism.checkMarkInstructionToSched(current_pair);
          ITT_TASK_END(checkMarkInstructionToSched);
          if (current_pair->second == instruction_state::STALL) {
            this->stallingInstruction = current_pair;
            SCMULATE_INFOMSG(5, "Stalling on %s", stallingInstruction->first->getFullInstruction().c_str());
          }
          // TIMERS_COUNTERS_GUARD(
          //   this->time_cnt_m->addEvent(this->su_timer_name, SU_IDLE););
          break;
        case instruction_state::READY:
          ready ++;
          current_pair->second = instruction_state::EXECUTING;
          switch (current_pair->first->getType()) {
            case COMMIT:
              SCMULATE_INFOMSG(4, "Scheduling and Exec a COMMIT");
              SCMULATE_INFOMSG(1, "Turning off machine alive = false");
              //#pragma omp atomic write
              *(this->aliveSignal) = false;
              // Properly clear the COMMIT instruction
              current_pair->second = instruction_state::DECOMMISSION;
              break;
            case CONTROL_INST:
              SCMULATE_INFOMSG(4, "Scheduling a CONTROL_INST %s", current_pair->first->getFullInstruction().c_str());
              // TIMERS_COUNTERS_GUARD(
              //     this->time_cnt_m->addEvent(this->su_timer_name, EXECUTE_CONTROL_INSTRUCTION, current_pair->first->getFullInstruction()););
              executeControlInstruction(current_pair->first);
              current_pair->second = instruction_state::EXECUTION_DONE;
              // TIMERS_COUNTERS_GUARD(
              //   this->time_cnt_m->addEvent(this->su_timer_name, SU_IDLE););
              break;
            case BASIC_ARITH_INST:
              SCMULATE_INFOMSG(4, "Scheduling a BASIC_ARITH_INST %s", current_pair->first->getFullInstruction().c_str());
              // TIMERS_COUNTERS_GUARD(
              //     this->time_cnt_m->addEvent(this->su_timer_name, EXECUTE_ARITH_INSTRUCTION););
              executeArithmeticInstructions(current_pair->first);
              current_pair->second = instruction_state::EXECUTION_DONE;
              // TIMERS_COUNTERS_GUARD(
              //   this->time_cnt_m->addEvent(this->su_timer_name, SU_IDLE););
              break;
            case EXECUTE_INST:
              SCMULATE_INFOMSG(4, "Scheduling an EXECUTE_INST %s", current_pair->first->getFullInstruction().c_str());
              if (!attemptAssignExecuteInstruction(current_pair))
                current_pair->second = instruction_state::READY;
              break;
            case MEMORY_INST:
              SCMULATE_INFOMSG(4, "Scheduling a MEMORY_INST %s", current_pair->first->getFullInstruction().c_str());
              if (!attemptAssignExecuteInstruction(current_pair))
                current_pair->second = instruction_state::READY;
              break;
            default:
              SCMULATE_ERROR(0, "Instruction not recognized");
              //#pragma omp atomic write
              *(this->aliveSignal) = false;
              break;
          }
          break;
        
        case instruction_state::EXECUTION_DONE:
          execution_done ++;
          TIMERS_COUNTERS_GUARD(
            this->time_cnt_m->addEvent(this->su_timer_name, DISPATCH_INSTRUCTION, current_pair->first->getFullInstruction()););
          // check if stalling instruction
          if (this->stallingInstruction != nullptr && this->stallingInstruction == current_pair) {
            SCMULATE_INFOMSG(5, "Unstalling on %s", stallingInstruction->first->getFullInstruction().c_str());
            this->stallingInstruction = nullptr;
          }
          ITT_TASK_BEGIN(fetch_decode_module_behavior, instructionFinished);
          instructionLevelParallelism.instructionFinished(current_pair);
          ITT_TASK_END(instructionFinished);
          SCMULATE_INFOMSG(5, "Marking instruction %s for decomision", current_pair->first->getFullInstruction().c_str());
          current_pair->second = instruction_state::DECOMMISSION;
          TIMERS_COUNTERS_GUARD(
            this->time_cnt_m->addEvent(this->su_timer_name, SU_IDLE, current_pair->first->getFullInstruction()););
          break;
        case instruction_state::EXECUTING:
          executing++;
          break;
        case instruction_state::DECOMMISSION:
          decomision++;
        default:
          break;
      }

    }

    // Check if any instructions have finished
    instructionLevelParallelism.printStats();
    SCMULATE_INFOMSG(6, "%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\n", stall, waiting, ready, execution_done, executing, decomision, this->inst_buff_m.getBufferSize());
    // Clear out instructions that are decomissioned
    this->inst_buff_m.clean_out_queue();

    // if (mark_event) {  
    //   TIMERS_COUNTERS_GUARD(
    //     this->time_cnt_m->addEvent(this->su_timer_name, SU_IDLE, std::string("PC = ") + std::to_string(PC)););
    // }
  }
  SCMULATE_INFOMSG(1, "Shutting down fetch decode unit");
  TIMERS_COUNTERS_GUARD(
      this->time_cnt_m->addEvent(this->su_timer_name, SU_END););
  return 0;
}

/* Variant of the behavior function that's adapted to only perform one iteration for usaged in the SU */
int scm::fetch_decode_module::tickBehavior()
{
  //ITT_DOMAIN(fetch_decode_module_behavior);
  //ITT_STR_HANDLE(checkMarkInstructionToSched);
  //ITT_STR_HANDLE(instructionFinished);
  uint64_t stall = 0, waiting = 0, ready = 0, execution_done = 0, executing = 0, decomision = 0;
  bool committed = false; 
  /*
  TIMERS_COUNTERS_GUARD(
      this->time_cnt_m->addEvent(this->su_timer_name, SU_START););
  SCMULATE_INFOMSG(1, "Initializing the SU");
   */
  //while (*(this->aliveSignal)) {
    // FETCHING PC
    int fetch_reps = 0;
    scm::decoded_instruction_t *new_inst = nullptr;
    if (!committed) {
      do {
        if (this->stallingInstruction == nullptr) {
          SCMULATE_INFOMSG(5, "FETCHING PC = %d", this->PC);
          new_inst = this->inst_mem_m->fetch(this->PC);
          if (!new_inst) {
            *(this->aliveSignal) = false;
            SCMULATE_ERROR(0, "Returned instruction is NULL for PC = %d. This should not happen", PC);
            continue;
          }
          // Insert new instruction
          if (this->inst_buff_m.add_instruction(*new_inst)) {
            //TIMERS_COUNTERS_GUARD(
              //this->time_cnt_m->addEvent(this->su_timer_name, FETCH_DECODE_INSTRUCTION, std::string("PC = ") + std::to_string(PC) + std::string(" ") + new_inst->getFullInstruction()););
            SCMULATE_INFOMSG(5, "Executing PC = %d", this->PC);
            //ITT_TASK_BEGIN(fetch_decode_module_behavior, checkMarkInstructionToSched);
            instructionLevelParallelism.checkMarkInstructionToSched(this->inst_buff_m.get_latest());
            //ITT_TASK_END(checkMarkInstructionToSched);
            if (this->inst_buff_m.get_latest()->second == instruction_state::STALL) {
                this->stallingInstruction = this->inst_buff_m.get_latest();
                SCMULATE_INFOMSG(5, "Stalling on %s", stallingInstruction->first->getFullInstruction().c_str());
            }

            // Mark instruction for scheduling
            committed = new_inst->getOpcode() == COMMIT_INST.opcode;
            SCMULATE_INFOMSG(5, "incrementing PC");
            this->PC++;
            //TIMERS_COUNTERS_GUARD(
              //this->time_cnt_m->addEvent(this->su_timer_name, SU_IDLE, std::string("PC = ") + std::to_string(PC)) );
          }
        } else {
          new_inst = this->stallingInstruction->first;
          if (this->stallingInstruction->second != instruction_state::STALL) {
            SCMULATE_INFOMSG(5, "Unstalling on %s", stallingInstruction->first->getFullInstruction().c_str());
            this->stallingInstruction = nullptr;
          }
        }
      } while (stallingInstruction == nullptr && !committed && ++fetch_reps < INSTRUCTION_FETCH_WINDOW && new_inst->getType() != instType::CONTROL_INST && new_inst->getType() != instType::COMMIT );
    }
    // Stats to collect
    stall = 0; waiting = 0; ready = 0; execution_done = 0; executing = 0; decomision = 0;
    // Iterate over the instruction buffer (window) looking for instructions to execute
    for (auto it = this->inst_buff_m.get_buffer()->begin(); it != this->inst_buff_m.get_buffer()->end(); ++it) {
      instruction_state_pair * current_pair = *it;
      switch (current_pair->second) {
        case instruction_state::STALL:
          stall++;
          //ITT_TASK_BEGIN(fetch_decode_module_behavior, checkMarkInstructionToSched);
          if (strcmp(current_pair->first->getInstruction().c_str(), "InitCod") != 0) {
            //current_pair->second = instruction_state::STALL;
            instructionLevelParallelism.checkMarkInstructionToSched(current_pair);
          } else if (!initScheduled) {

                initScheduled = gemAttemptAssignExecuteInstruction(current_pair);
          }
          //instructionLevelParallelism.checkMarkInstructionToSched(current_pair);
         
          // if this is a stalled arithmetic instruction and SU isn't currently fetching, trigger the fetch
          /*
          if (current_pair->first->getType() == BASIC_ARITH_INST && owner->getStallingInst() == nullptr) {
            fetchOperandsFromMem(current_pair);
          }
          // if this instruction is stalling because SU is currently fetching reg data for it, make sure it stays stalling
          // and perform further calls
          else if (current_pair->first->getType() == BASIC_ARITH_INST && owner->getStallingInst() == current_pair) {
            current_pair->second = instruction_state::STALL;
            // if operands are done being fetched by SU
            if (owner->getCopyState() == gem5::SU::FETCH_COMPLETE) {
              // TODO: need to get the result here from the execution to send the data for write back as a parameter and modify the execute function
              uint64_t result = *((uint64_t *)execArithInstFromCopy());
              uint64_t * result_ptr = new uint64_t;
              memcpy(result_ptr, &result, sizeof(uint64_t));
              //executeArithmeticInstructions(current_pair->first); // perform actual computation based on SU's local register copies
              owner->writebackOpToMem(result_ptr); // trigger the writeback operation
            } else if (owner->getCopyState() == gem5::SU::TRANS_COMPLETE) { // if full transaction is complete, i.e. writeback is done
              // set instruction to complete
              current_pair->second = instruction_state::EXECUTION_DONE;
              // clear the SU's stalling instruction and reset copy state since we're done
              owner->clearStallingInst();
            }
          } 
          */
          //ITT_TASK_END(checkMarkInstructionToSched);
          if (current_pair->second == instruction_state::STALL)
            this->stallingInstruction = current_pair;
          break;
        case instruction_state::WAITING:
          waiting++;
          //ITT_TASK_BEGIN(fetch_decode_module_behavior, checkMarkInstructionToSched);
          instructionLevelParallelism.checkMarkInstructionToSched(current_pair);
          //ITT_TASK_END(checkMarkInstructionToSched);
          if (current_pair->second == instruction_state::STALL) {
            this->stallingInstruction = current_pair;
            SCMULATE_INFOMSG(5, "Stalling on %s", stallingInstruction->first->getFullInstruction().c_str());
          }
          break;
        case instruction_state::READY:
          ready ++;
          current_pair->second = instruction_state::EXECUTING;
          switch (current_pair->first->getType()) {
            case COMMIT:
              SCMULATE_INFOMSG(4, "Scheduling and Exec a COMMIT");
              SCMULATE_INFOMSG(1, "Turning off machine alive = false");
              gemAttemptAssignCommit();
              // only kill machine and decomision instruction if SU accepts the commit call
              *(this->aliveSignal) = false;
              // Properly clear the COMMIT instruction
              current_pair->second = instruction_state::DECOMMISSION;
              break;
            case CONTROL_INST:
              SCMULATE_INFOMSG(4, "Scheduling a CONTROL_INST %s", current_pair->first->getFullInstruction().c_str());
              executeControlInstruction(current_pair->first);
              current_pair->second = instruction_state::EXECUTION_DONE;
              break;
            case BASIC_ARITH_INST:
              SCMULATE_INFOMSG(4, "Scheduling a BASIC_ARITH_INST %s", current_pair->first->getFullInstruction().c_str());
              executeArithmeticInstructions(current_pair->first);
              current_pair->second = instruction_state::EXECUTION_DONE;
              // Arithmetic instructions now have to stall so the SU has time to fetch the data from memory, perform the operation, and write back
              //current_pair->second = instruction_state::STALL; // set to stall in instruction mem
              //if (owner->getStallingInst() == nullptr) { //stallingInst being nullptr means no fetch is going on in the SU currently
              //  fetchOperandsFromMem(current_pair); // trigger register reading operation in the SU
              //}
              break;
            case EXECUTE_INST:
              SCMULATE_INFOMSG(4, "Scheduling an EXECUTE_INST %s", current_pair->first->getFullInstruction().c_str());
              SCMULATE_INFOMSG(4, "EXECUTE_INST has getInstruction: %s", current_pair->first->getInstruction().c_str());
              if (strcmp(current_pair->first->getInstruction().c_str(), "InitCod_64B")==0) {
                current_pair->second = instruction_state::STALL;
                stallingInstruction = current_pair;
                initScheduled = gemAttemptAssignExecuteInstruction(current_pair);
              } else if (!gemAttemptAssignExecuteInstruction(current_pair)) {
                current_pair->second = instruction_state::READY;
              }
              break;
            case MEMORY_INST:
              SCMULATE_INFOMSG(4, "Scheduling a MEMORY_INST %s", current_pair->first->getFullInstruction().c_str());
              //if (!gemAttemptAssignExecuteInstruction(current_pair))
              //  current_pair->second = instruction_state::READY;
              executeMemoryInstruction(current_pair);
              current_pair->second = instruction_state::EXECUTION_DONE;
              break;
            default:
              SCMULATE_ERROR(0, "Instruction not recognized");
              *(this->aliveSignal) = false;
              break;
          }
          break;
        
        case instruction_state::EXECUTION_DONE:
          execution_done ++;
          //TIMERS_COUNTERS_GUARD(
            //this->time_cnt_m->addEvent(this->su_timer_name, DISPATCH_INSTRUCTION, current_pair->first->getFullInstruction()););
          // check if stalling instruction
          if (this->stallingInstruction != nullptr && this->stallingInstruction == current_pair) {
            SCMULATE_INFOMSG(5, "Unstalling on %s", stallingInstruction->first->getFullInstruction().c_str());
            this->stallingInstruction = nullptr;
          }
          //ITT_TASK_BEGIN(fetch_decode_module_behavior, instructionFinished);
          instructionLevelParallelism.instructionFinished(current_pair);
          //ITT_TASK_END(instructionFinished);
          SCMULATE_INFOMSG(5, "Marking instruction %s for decomision", current_pair->first->getFullInstruction().c_str());
          current_pair->second = instruction_state::DECOMMISSION;
          //TIMERS_COUNTERS_GUARD(
            //this->time_cnt_m->addEvent(this->su_timer_name, SU_IDLE, current_pair->first->getFullInstruction()););
          break;
        case instruction_state::EXECUTING:
          executing++;
          break;
        case instruction_state::DECOMMISSION:
          decomision++;
        default:
          break;
      }

    }

    // Check if any instructions have finished
    //instructionLevelParallelism.printStats();
    //SCMULATE_INFOMSG(6, "%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\n", stall, waiting, ready, execution_done, executing, decomision, this->inst_buff_m.getBufferSize());
    // Clear out instructions that are decomissioned
    this->inst_buff_m.clean_out_queue();

  //} // while alive sig
  //SCMULATE_INFOMSG(1, "Shutting down fetch decode unit");
  //TIMERS_COUNTERS_GUARD(
      //this->time_cnt_m->addEvent(this->su_timer_name, SU_END););
  return 0;
}

void scm::fetch_decode_module::executeControlInstruction(scm::decoded_instruction_t *inst)
{

  /////////////////////////////////////////////////////
  ///// CONTROL LOGIC FOR THE JMPLBL INSTRUCTION
  /////////////////////////////////////////////////////
  if (inst->getOpcode() == JMPLBL_INST.opcode) {
    int newPC = inst->getOp(1).value.immediate;
    SCMULATE_ERROR_IF(0, newPC == -1, "Incorrect label translation");
    PC = newPC;
    return;
  }
  /////////////////////////////////////////////////////
  ///// CONTROL LOGIC FOR THE JMPPC INSTRUCTION
  /////////////////////////////////////////////////////
  if (inst->getOpcode() == JMPPC_INST.opcode) {
    int offset = inst->getOp1().value.immediate;
    int target = offset + PC - 1;
    SCMULATE_ERROR_IF(0, ((uint32_t)target > this->inst_mem_m->getMemSize() || target < 0), "Incorrect destination offset");
    PC = target;
    return;
  }
  /////////////////////////////////////////////////////
  ///// CONTROL LOGIC FOR THE BREQ INSTRUCTION
  /////////////////////////////////////////////////////
  if (inst->getOpcode() == BREQ_INST.opcode) {
    decoded_reg_t reg1 = inst->getOp1().value.reg;
    decoded_reg_t reg2 = inst->getOp2().value.reg;
    //unsigned char *reg1_ptr = reg1.reg_ptr;
    //unsigned char *reg2_ptr = reg2.reg_ptr;
    unsigned char * reg1_ptr = (unsigned char *) owner->fetchOp(&reg1);
    unsigned char * reg2_ptr = (unsigned char *) owner->fetchOp(&reg2);
    SCMULATE_INFOMSG(4, "Comparing register %s %d to %s %d", reg1.reg_size.c_str(), reg1.reg_number, reg2.reg_size.c_str(), reg2.reg_number);
    bool bitComparison = true;
    SCMULATE_ERROR_IF(0, reg1.reg_size != reg2.reg_size, "Attempting to compare registers of different size");
    for (uint32_t i = 0; i < reg1.reg_size_bytes; ++i) {
      if (reg1_ptr[i] ^ reg2_ptr[i]) {
        bitComparison = false;
        break;
      }
    }
    if (bitComparison) {
      int target;
      if (inst->getOp(3).type == operand_t::LABEL) {
        target = inst->getOp(3).value.immediate;
      } else {
        int offset = inst->getOp(3).value.immediate;
        target = offset + PC - 1;
      }
      SCMULATE_ERROR_IF(0, ((uint32_t)target > this->inst_mem_m->getMemSize() || target < 0), "Incorrect destination offset");
      PC = target;
    }
    delete [] reg1_ptr;
    delete [] reg2_ptr;
    return;
  }
  /////////////////////////////////////////////////////
  ///// CONTROL LOGIC FOR THE BGT INSTRUCTION
  /////////////////////////////////////////////////////
  if (inst->getOpcode() == BGT_INST.opcode) {
    decoded_reg_t reg1 = inst->getOp1().value.reg;
    decoded_reg_t reg2 = inst->getOp2().value.reg;
    //unsigned char *reg1_ptr = reg1.reg_ptr;
    //unsigned char *reg2_ptr = reg2.reg_ptr;
    unsigned char *reg1_ptr = (unsigned char *) owner->fetchOp(&reg1);
    unsigned char *reg2_ptr = (unsigned char *) owner->fetchOp(&reg2);
    SCMULATE_INFOMSG(4, "Comparing register %s %d to %s %d", reg1.reg_size.c_str(), reg1.reg_number, reg2.reg_size.c_str(), reg2.reg_number);
    bool reg1_gt_reg2 = false;
    SCMULATE_ERROR_IF(0, reg1.reg_size != reg2.reg_size, "Attempting to compare registers of different size");
    for (uint32_t i = 0; i < reg1.reg_size_bytes; ++i) {
      // Find the first byte from MSB to LSB that is different in reg1 and reg2. If reg1 > reg2 in that byte, then reg1 > reg2 in general
      if (reg1_ptr[i] ^ reg2_ptr[i] && reg1_ptr[i] > reg2_ptr[i]) {
        reg1_gt_reg2 = true;
        break;
      }
    }
    if (reg1_gt_reg2) {
      int target;
      if (inst->getOp(3).type == operand_t::LABEL) {
        target = inst->getOp(3).value.immediate;
      } else {
        int offset = inst->getOp(3).value.immediate;
        target = offset + PC - 1;
      }
      SCMULATE_ERROR_IF(0, ((uint32_t)target > this->inst_mem_m->getMemSize() || target < 0), "Incorrect destination offset");
      PC = target;
    }
    delete [] reg1_ptr;
    delete [] reg2_ptr;
    return;
  }
  /////////////////////////////////////////////////////
  ///// CONTROL LOGIC FOR THE BGET INSTRUCTION
  /////////////////////////////////////////////////////
  if (inst->getOpcode() == BGET_INST.opcode) {
    decoded_reg_t reg1 = inst->getOp1().value.reg;
    decoded_reg_t reg2 = inst->getOp2().value.reg;
    //unsigned char *reg1_ptr = reg1.reg_ptr;
    //unsigned char *reg2_ptr = reg2.reg_ptr;
    unsigned char *reg1_ptr = (unsigned char *) owner->fetchOp(&reg1);
    unsigned char *reg2_ptr = (unsigned char *) owner->fetchOp(&reg2);
    SCMULATE_INFOMSG(4, "Comparing register %s %d to %s %d", reg1.reg_size.c_str(), reg1.reg_number, reg2.reg_size.c_str(), reg2.reg_number);
    bool reg1_get_reg2 = false;
    SCMULATE_ERROR_IF(0, reg1.reg_size != reg2.reg_size, "Attempting to compare registers of different size");
    uint32_t size_reg_bytes = reg1.reg_size_bytes;
    for (uint32_t i = 0; i < size_reg_bytes; ++i) {
      // Find the first byte from MSB to LSB that is different in reg1 and reg2. If reg1 > reg2 in that byte, then reg1 > reg2 in general
      if (reg1_ptr[i] ^ reg2_ptr[i] && reg1_ptr[i] > reg2_ptr[i]) {
        reg1_get_reg2 = true;
        break;
      }
      // If we have not found any byte that is different in both registers from MSB to LSB, and the LSB byte is the same, the the registers are the same
      if (i == size_reg_bytes - 1 && reg1_ptr[i] == reg2_ptr[i])
        reg1_get_reg2 = true;
    }
    if (reg1_get_reg2) {
      int target;
      if (inst->getOp(3).type == operand_t::LABEL) {
        target = inst->getOp(3).value.immediate;
      } else {
        int offset = inst->getOp(3).value.immediate;
        target = offset + PC - 1;
      }
      SCMULATE_ERROR_IF(0, ((uint32_t)target > this->inst_mem_m->getMemSize() || target < 0), "Incorrect destination offset");
      PC = target;
    }
    delete [] reg1_ptr;
    delete [] reg2_ptr;
    return;
  }
  /////////////////////////////////////////////////////
  ///// CONTROL LOGIC FOR THE BLT INSTRUCTION
  /////////////////////////////////////////////////////
  if (inst->getOpcode() == BLT_INST.opcode) {
    decoded_reg_t reg1 = inst->getOp1().value.reg;
    decoded_reg_t reg2 = inst->getOp2().value.reg;
    //unsigned char *reg1_ptr = reg1.reg_ptr;
    //unsigned char *reg2_ptr = reg2.reg_ptr;
    unsigned char *reg1_ptr = (unsigned char *) owner->fetchOp(&reg1);
    unsigned char *reg2_ptr = (unsigned char *) owner->fetchOp(&reg2);
    SCMULATE_INFOMSG(4, "Comparing register %s %d to %s %d", reg1.reg_size.c_str(), reg1.reg_number, reg2.reg_size.c_str(), reg2.reg_number);
    bool reg1_lt_reg2 = false;
    SCMULATE_ERROR_IF(0, reg1.reg_size != reg2.reg_size, "Attempting to compare registers of different size");
    for (uint32_t i = 0; i < reg1.reg_size_bytes; ++i) {
      // Find the first byte from MSB to LSB that is different in reg1 and reg2. If reg1 < reg2 in that byte, then reg1 < reg2 in general
      if (reg1_ptr[i] ^ reg2_ptr[i] && reg1_ptr[i] < reg2_ptr[i]) {
        reg1_lt_reg2 = true;
        break;
      }
    }
    if (reg1_lt_reg2) {
      int target;
      if (inst->getOp(3).type == operand_t::LABEL) {
        target = inst->getOp(3).value.immediate;
      } else {
        int offset = inst->getOp(3).value.immediate;
        target = offset + PC - 1;
      }
      SCMULATE_ERROR_IF(0, ((uint32_t)target > this->inst_mem_m->getMemSize() || target < 0), "Incorrect destination offset");
      PC = target;
    }
    delete [] reg1_ptr;
    delete [] reg2_ptr;
    return;
  }
  /////////////////////////////////////////////////////
  ///// CONTROL LOGIC FOR THE BLET INSTRUCTION
  /////////////////////////////////////////////////////
  if (inst->getOpcode() == BLET_INST.opcode) {
    decoded_reg_t reg1 = inst->getOp1().value.reg;
    decoded_reg_t reg2 = inst->getOp2().value.reg;
    //unsigned char *reg1_ptr = reg1.reg_ptr;
    //unsigned char *reg2_ptr = reg2.reg_ptr;
    unsigned char *reg1_ptr = (unsigned char *) owner->fetchOp(&reg1);
    unsigned char *reg2_ptr = (unsigned char *) owner->fetchOp(&reg2);
    SCMULATE_INFOMSG(4, "Comparing register %s %d to %s %d", reg1.reg_size.c_str(), reg1.reg_number, reg2.reg_size.c_str(), reg2.reg_number);
    bool reg1_let_reg2 = false;
    SCMULATE_ERROR_IF(0, reg1.reg_size != reg2.reg_size, "Attempting to compare registers of different size");
    uint32_t size_reg_bytes = reg1.reg_size_bytes;
    for (uint32_t i = 0; i < size_reg_bytes; ++i) {
      // Find the first byte from MSB to LSB that is different in reg1 and reg2. If reg1 < reg2 in that byte, then reg1 < reg2 in general
      if (reg1_ptr[i] ^ reg2_ptr[i] && reg1_ptr[i] < reg2_ptr[i]) {
        reg1_let_reg2 = true;
        break;
      }
      // If we have not found any byte that is different in both registers from MSB to LSB, and the LSB byte is the same, the the registers are the same
      if (i == size_reg_bytes - 1 && reg1_ptr[i] == reg2_ptr[i])
        reg1_let_reg2 = true;
    }
    if (reg1_let_reg2) {
      int target;
      if (inst->getOp(3).type == operand_t::LABEL) {
        target = inst->getOp(3).value.immediate;
      } else {
        int offset = inst->getOp(3).value.immediate;
        target = offset + PC - 1;
      }
      SCMULATE_ERROR_IF(0, ((uint32_t)target > this->inst_mem_m->getMemSize() || target < 0), "Incorrect destination offset");
      PC = target;
    }
    delete [] reg1_ptr;
    delete [] reg2_ptr;
    return;
  }
}

unsigned char * scm::fetch_decode_module::execArithInstFromCopy()
{
  scm::decoded_instruction_t * inst = owner->getStallingInst()->first;
  /////////////////////////////////////////////////////
  ///// ARITHMETIC LOGIC FOR THE ADD INSTRUCTION
  /////////////////////////////////////////////////////
  if (inst->getOpcode() == ADD_INST.opcode)
  {
    decoded_reg_t reg1 = inst->getOp1().value.reg;
    //decoded_reg_t reg2 = inst->getOp2().value.reg;
    // Second operand may be register or immediate. We assumme immediate are no longer than a long long
    if (inst->getOp3().type == scm::operand_t::IMMEDIATE_VAL)
    {
      // IMMEDIATE ADDITION CASE
      // TODO: Think about the signed option of these operands
      uint64_t immediate_val = inst->getOp3().value.immediate;

      //unsigned char *reg2_ptr = reg2.reg_ptr;
      unsigned char *reg2_ptr = owner->getLocalSrc1Ptr();
      SCMULATE_INFOMSG(3, "Src1 copy is : %ld", *((uint64_t *)reg2_ptr));
      SCMULATE_INFOMSG(3, "Immediate value is : %ld", immediate_val);

      // Where to store the result
      //unsigned char *reg1_ptr = reg1.reg_ptr;
      unsigned char *reg1_ptr = owner->getLocalDestPtr();
      int32_t size_reg_bytes = reg1.reg_size_bytes;
      assert(size_reg_bytes <= 8);

      // Addition
      uint32_t temp = 0;
      //for (int32_t i = size_reg_bytes - 1; i >= 0; --i)
      for (int32_t i=0; i < size_reg_bytes; ++i)
      {
        temp += (immediate_val & 255) + (reg2_ptr[i]);
        reg1_ptr[i] = temp & 255;
        immediate_val >>= 8;
        // Carry on
        temp = temp > 255 ? 1 : 0;
        SCMULATE_INFOMSG(3, "Intermediate dest is : %ld", *((uint64_t *)reg1_ptr));
      }
      SCMULATE_INFOMSG(3, "Dest copy is : %ld", *((uint64_t *)reg1_ptr));
      return reg1_ptr;
    }
    else
    {
      // REGISTER REGISTER ADD CASE
      decoded_reg_t reg3 = inst->getOp3().value.reg;
      //unsigned char *reg2_ptr = reg2.reg_ptr;
      unsigned char *reg2_ptr = owner->getLocalSrc1Ptr();
      //unsigned char *reg3_ptr = reg3.reg_ptr;
      unsigned char *reg3_ptr = owner->getLocalSrc2Ptr();
      SCMULATE_INFOMSG(3, "Src1 copy is : %ld", *((uint64_t *)reg2_ptr));
      SCMULATE_INFOMSG(3, "Src2 copy is : %ld", *((uint64_t *)reg3_ptr));

      // Where to store the result
      //unsigned char *reg1_ptr = reg1.reg_ptr;
      unsigned char *reg1_ptr = owner->getLocalDestPtr();
      int32_t size_reg_bytes = reg1.reg_size_bytes;
      assert(size_reg_bytes <= 8);

      // Addition
      int temp = 0;
      for (int32_t i = size_reg_bytes - 1; i >= 0; --i)
      {
        temp += (reg3_ptr[i]) + (reg2_ptr[i]);
        reg1_ptr[i] = temp & 255;
        // Carry on
        temp = temp > 255 ? 1 : 0;
      }
      SCMULATE_INFOMSG(3, "Dest copy is : %ld", *((uint64_t *)reg1_ptr));
      return reg1_ptr;
    }
  }

  /////////////////////////////////////////////////////
  ///// ARITHMETIC LOGIC FOR THE SUB INSTRUCTION
  /////////////////////////////////////////////////////
  if (inst->getOpcode() == SUB_INST.opcode)
  {
    decoded_reg_t reg1 = inst->getOp1().value.reg;
    decoded_reg_t reg2 = inst->getOp2().value.reg;

    // Second operand may be register or immediate. We assumme immediate are no longer than a long long
    if (inst->getOp3().type == scm::operand_t::IMMEDIATE_VAL)
    {
      // IMMEDIATE ADDITION CASE
      // TODO: Think about the signed option of these operands
      uint16_t immediate_val = inst->getOp3().value.immediate;

      unsigned char *reg2_ptr = reg2.reg_ptr;

      // Where to store the result
      unsigned char *reg1_ptr = reg1.reg_ptr;
      int32_t size_reg_bytes = reg1.reg_size_bytes;

      // Subtraction
      uint32_t temp = 0;
      for (int32_t i = size_reg_bytes - 1; i >= 0; --i)
      {
        uint32_t cur_byte = immediate_val & 255;
        if (reg2_ptr[i] < cur_byte + temp)
        {
          reg1_ptr[i] = reg2_ptr[i] + 256 - temp - cur_byte;
          temp = 1; // Increase carry
        }
        else
        {
          reg1_ptr[i] = reg2_ptr[i] - temp - cur_byte;
          temp = 0; // Carry has been used
        }
        immediate_val >>= 8;
      }
      SCMULATE_ERROR_IF(0, temp == 1, "Registers must be possitive numbers, addition of numbers resulted in negative number. Carry was 1 at the end of the operation");
    }
    else
    {
      // REGISTER REGISTER ADD CASE
      decoded_reg_t reg3 = inst->getOp3().value.reg;
      unsigned char *reg2_ptr = reg2.reg_ptr;
      unsigned char *reg3_ptr = reg3.reg_ptr;

      // Where to store the result
      unsigned char *reg1_ptr = reg1.reg_ptr;
      int32_t size_reg_bytes = reg1.reg_size_bytes;

      // Subtraction
      uint32_t temp = 0;
      for (int32_t i = size_reg_bytes - 1; i >= 0; --i)
      {
        if (reg2_ptr[i] < reg3_ptr[i] + temp)
        {
          reg1_ptr[i] = reg2_ptr[i] + 256 - temp - reg3_ptr[i];
          temp = 1; // Increase carry
        }
        else
        {
          reg1_ptr[i] = reg2_ptr[i] - temp - reg3_ptr[i];
          temp = 0; // Carry has been used
        }
      }
      SCMULATE_ERROR_IF(0, temp == 1, "Registers must be possitive numbers, addition of numbers resulted in negative number. Carry was 1 at the end of the operation");
    }
    return nullptr;
  }

  /////////////////////////////////////////////////////
  ///// ARITHMETIC LOGIC FOR THE SHFL INSTRUCTION
  /////////////////////////////////////////////////////
  if (inst->getOpcode() == SHFL_INST.opcode)
  {
    SCMULATE_ERROR(0, "THE SHFL OPERATION HAS NOT BEEN IMPLEMENTED. KILLING THIS")
//#pragma omp atomic write
    *(this->aliveSignal) = false;
  }

  /////////////////////////////////////////////////////
  ///// ARITHMETIC LOGIC FOR THE SHFR INSTRUCTION
  /////////////////////////////////////////////////////
  if (inst->getOpcode() == SHFR_INST.opcode)
  {
    SCMULATE_ERROR(0, "THE SHFR OPERATION HAS NOT BEEN IMPLEMENTED. KILLING THIS")
//#pragma omp atomic write
    *(this->aliveSignal) = false;
  }

  /////////////////////////////////////////////////////
  ///// ARITHMETIC LOGIC FOR THE MULT INSTRUCTION
  /////////////////////////////////////////////////////
  if (inst->getOpcode() == MULT_INST.opcode) {
    decoded_reg_t reg1 = inst->getOp(1).value.reg;
    decoded_reg_t reg2 = inst->getOp(2).value.reg;
    uint64_t op2_val = 0;
    uint64_t op3_val = 0;

    // Get value for reg2
    unsigned char *reg2_ptr = reg2.reg_ptr;
    int32_t size_reg2_bytes = reg2.reg_size_bytes;
    for (int32_t i = 0; i < size_reg2_bytes; ++i) {
      op2_val <<= 8;
      op2_val += static_cast<uint8_t>(reg2_ptr[i]);
    }
    // Third operand may be register or immediate. We assumme immediate are no longer than a long long
    if (inst->getOp(3).type == scm::operand_t::IMMEDIATE_VAL) {
      // IMMEDIATE MULT CASE
      // TODO: Think about the signed option of these operands
      op3_val = inst->getOp(3).value.immediate;
    } else {
      // REGISTER REGISTER ADD CASE
      decoded_reg_t reg3 = inst->getOp3().value.reg;
      unsigned char *reg3_ptr = reg3.reg_ptr;
      int32_t size_reg3_bytes = reg3.reg_size_bytes;
      for (int32_t i = 0; i < size_reg3_bytes; ++i) {
        op3_val <<= 8;
        op3_val += static_cast<uint8_t>(reg3_ptr[i]);
      }
    }

    uint64_t mult_res = op2_val * op3_val;
    // Where to store the result
    unsigned char *reg1_ptr = reg1.reg_ptr;
    int32_t size_reg1_bytes = reg1.reg_size_bytes;
    for (int32_t i = size_reg1_bytes - 1; i >= 0; --i) {
      reg1_ptr[i] = mult_res & 255;
      mult_res >>= 8;
    }
    return nullptr;
  }
}

void scm::fetch_decode_module::executeArithmeticInstructions(scm::decoded_instruction_t *inst)
{
  /////////////////////////////////////////////////////
  ///// ARITHMETIC LOGIC FOR THE ADD INSTRUCTION
  /////////////////////////////////////////////////////
  if (inst->getOpcode() == ADD_INST.opcode)
  {
    decoded_reg_t reg1 = inst->getOp1().value.reg;
    decoded_reg_t reg2 = inst->getOp2().value.reg;
    // Second operand may be register or immediate. We assumme immediate are no longer than a long long
    if (inst->getOp3().type == scm::operand_t::IMMEDIATE_VAL)
    {
      // IMMEDIATE ADDITION CASE
      // TODO: Think about the signed option of these operands
      uint64_t immediate_val = inst->getOp3().value.immediate;

      //unsigned char *reg2_ptr = reg2.reg_ptr;
      //unsigned char *reg2_ptr = (unsigned char *) owner->fetchOp(&reg2);
      uint64_t * reg2_ptr = (uint64_t *) owner->fetchOp(&reg2);

      // Where to store the result
      //unsigned char *reg1_ptr = reg1.reg_ptr;
      int32_t size_reg_bytes = reg1.reg_size_bytes;

      // Addition
      /*
      uint32_t temp = 0;
      for (int32_t i = size_reg_bytes - 1; i >= 0; --i)
      {
        temp += (immediate_val & 255) + (reg2_ptr[i]);
        reg1_ptr[i] = temp & 255;
        immediate_val >>= 8;
        // Carry on
        temp = temp > 255 ? 1 : 0;
      }
      */ 
      uint64_t temp = *reg2_ptr + immediate_val;
      owner->writeOp(&reg1, (void *)&temp);
      delete [] reg2_ptr;
    }
    else
    {
      // REGISTER REGISTER ADD CASE
      decoded_reg_t reg3 = inst->getOp3().value.reg;
      //unsigned char *reg2_ptr = reg2.reg_ptr;
      //unsigned char *reg3_ptr = reg3.reg_ptr;
      uint64_t * reg2_ptr = (uint64_t *) owner->fetchOp(&reg2);
      uint64_t * reg3_ptr = (uint64_t *) owner->fetchOp(&reg3);

      // Where to store the result
      //unsigned char *reg1_ptr = reg1.reg_ptr;
      int32_t size_reg_bytes = reg1.reg_size_bytes;

      // Addition
      /*
      int temp = 0;
      for (int32_t i = size_reg_bytes - 1; i >= 0; --i)
      {
        temp += (reg3_ptr[i]) + (reg2_ptr[i]);
        reg1_ptr[i] = temp & 255;
        // Carry on
        temp = temp > 255 ? 1 : 0;
      }
      */ 
      uint64_t temp = *reg2_ptr + *reg3_ptr;
      owner->writeOp(&reg1, (void *)&temp);
      delete [] reg2_ptr;
      delete [] reg3_ptr;
    }
    return;
  }

  /////////////////////////////////////////////////////
  ///// ARITHMETIC LOGIC FOR THE SUB INSTRUCTION
  /////////////////////////////////////////////////////
  if (inst->getOpcode() == SUB_INST.opcode)
  {
    decoded_reg_t reg1 = inst->getOp1().value.reg;
    decoded_reg_t reg2 = inst->getOp2().value.reg;

    // Second operand may be register or immediate. We assumme immediate are no longer than a long long
    if (inst->getOp3().type == scm::operand_t::IMMEDIATE_VAL)
    {
      // IMMEDIATE ADDITION CASE
      // TODO: Think about the signed option of these operands
      uint16_t immediate_val = inst->getOp3().value.immediate;

      //unsigned char *reg2_ptr = reg2.reg_ptr;
      uint64_t * reg2_ptr = (uint64_t *) owner->fetchOp(&reg2);

      // Where to store the result
      unsigned char *reg1_ptr = reg1.reg_ptr;
      int32_t size_reg_bytes = reg1.reg_size_bytes;

      // Subtraction
      /*
      uint32_t temp = 0;
      for (int32_t i = size_reg_bytes - 1; i >= 0; --i)
      {
        uint32_t cur_byte = immediate_val & 255;
        if (reg2_ptr[i] < cur_byte + temp)
        {
          reg1_ptr[i] = reg2_ptr[i] + 256 - temp - cur_byte;
          temp = 1; // Increase carry
        }
        else
        {
          reg1_ptr[i] = reg2_ptr[i] - temp - cur_byte;
          temp = 0; // Carry has been used
        }
        immediate_val >>= 8;
      }
      */ 
      uint64_t temp = *reg2_ptr - immediate_val;
      SCMULATE_INFOMSG(3, "SUB: %lx - %lx = %lx", *reg2_ptr, immediate_val, temp);
      SCMULATE_ERROR_IF(0, temp == 1, "Registers must be possitive numbers, addition of numbers resulted in negative number. Carry was 1 at the end of the operation");
      owner->writeOp(&reg1, (void *)&temp);
      delete [] reg2_ptr;
    }
    else
    {
      // REGISTER REGISTER ADD CASE
      decoded_reg_t reg3 = inst->getOp3().value.reg;
      //unsigned char *reg2_ptr = reg2.reg_ptr;
      //unsigned char *reg3_ptr = reg3.reg_ptr;
      uint64_t * reg2_ptr = (uint64_t *) owner->fetchOp(&reg2);
      uint64_t * reg3_ptr = (uint64_t *) owner->fetchOp(&reg3);

      // Where to store the result
      //unsigned char *reg1_ptr = reg1.reg_ptr;
      int32_t size_reg_bytes = reg1.reg_size_bytes;

      // Subtraction
      /*
      uint32_t temp = 0;
      for (int32_t i = size_reg_bytes - 1; i >= 0; --i)
      {
        if (reg2_ptr[i] < reg3_ptr[i] + temp)
        {
          reg1_ptr[i] = reg2_ptr[i] + 256 - temp - reg3_ptr[i];
          temp = 1; // Increase carry
        }
        else
        {
          reg1_ptr[i] = reg2_ptr[i] - temp - reg3_ptr[i];
          temp = 0; // Carry has been used
        }
      }
      */ 
      uint64_t temp = *reg2_ptr - *reg3_ptr;
      SCMULATE_INFOMSG(3, "SUB: %lx - %lx = %lx", *reg2_ptr, *reg3_ptr, temp);
      SCMULATE_ERROR_IF(0, temp == 1, "Registers must be possitive numbers, addition of numbers resulted in negative number. Carry was 1 at the end of the operation");
      owner->writeOp(&reg1, &temp);
      delete [] reg2_ptr;
      delete [] reg3_ptr;
    }
    return;
  }

  /////////////////////////////////////////////////////
  ///// ARITHMETIC LOGIC FOR THE SHFL INSTRUCTION
  /////////////////////////////////////////////////////
  if (inst->getOpcode() == SHFL_INST.opcode)
  {
    SCMULATE_ERROR(0, "THE SHFL OPERATION HAS NOT BEEN IMPLEMENTED. KILLING THIS")
//#pragma omp atomic write
    *(this->aliveSignal) = false;
  }

  /////////////////////////////////////////////////////
  ///// ARITHMETIC LOGIC FOR THE SHFR INSTRUCTION
  /////////////////////////////////////////////////////
  if (inst->getOpcode() == SHFR_INST.opcode)
  {
    SCMULATE_ERROR(0, "THE SHFR OPERATION HAS NOT BEEN IMPLEMENTED. KILLING THIS")
//#pragma omp atomic write
    *(this->aliveSignal) = false;
  }

  /////////////////////////////////////////////////////
  ///// ARITHMETIC LOGIC FOR THE MULT INSTRUCTION
  /////////////////////////////////////////////////////
  if (inst->getOpcode() == MULT_INST.opcode) {
    decoded_reg_t reg1 = inst->getOp(1).value.reg;
    decoded_reg_t reg2 = inst->getOp(2).value.reg;
    uint64_t op2_val = 0;
    uint64_t op3_val = 0;

    // Get value for reg2
    /*
    unsigned char *reg2_ptr = reg2.reg_ptr;
    int32_t size_reg2_bytes = reg2.reg_size_bytes;
    for (int32_t i = 0; i < size_reg2_bytes; ++i) {
      op2_val <<= 8;
      op2_val += static_cast<uint8_t>(reg2_ptr[i]);
    }
    */
    uint64_t * reg2_ptr = (uint64_t *) owner->fetchOp(&reg2);
    op2_val = *reg2_ptr;
    delete [] reg2_ptr;
    // Third operand may be register or immediate. We assumme immediate are no longer than a long long
    if (inst->getOp(3).type == scm::operand_t::IMMEDIATE_VAL) {
      // IMMEDIATE MULT CASE
      // TODO: Think about the signed option of these operands
      op3_val = inst->getOp(3).value.immediate;
    } else {
      // REGISTER REGISTER ADD CASE
      decoded_reg_t reg3 = inst->getOp3().value.reg;
      /*
      unsigned char *reg3_ptr = reg3.reg_ptr;
      int32_t size_reg3_bytes = reg3.reg_size_bytes;
      for (int32_t i = 0; i < size_reg3_bytes; ++i) {
        op3_val <<= 8;
        op3_val += static_cast<uint8_t>(reg3_ptr[i]);
      }
      */
      uint64_t * reg3_ptr = (uint64_t *) owner->fetchOp(&reg3);
      op3_val = *reg3_ptr;
      delete [] reg3_ptr;
    }

    uint64_t mult_res = op2_val * op3_val;
    // Where to store the result
    /*
    unsigned char *reg1_ptr = reg1.reg_ptr;
    int32_t size_reg1_bytes = reg1.reg_size_bytes;
    for (int32_t i = size_reg1_bytes - 1; i >= 0; --i) {
      reg1_ptr[i] = mult_res & 255;
      mult_res >>= 8;
    }
    */ 
    // with multiplication like this we have to beware over overflows
    uint64_t temp = op2_val * op3_val;
    owner->writeOp(&reg1, (void *) &temp);
    return;
  }
}

bool scm::fetch_decode_module::attemptAssignExecuteInstruction(scm::instruction_state_pair *inst)
{
  // TODO: Jose this is the point where you can select scheduing policies
  static uint32_t curSched = 0;
  bool sched = false;
  uint32_t attempts = 0;
  // We try scheduling on all the sched units
  while (!sched && attempts++ < this->ctrl_st_m->numExecutors()) {
    if (this->ctrl_st_m->get_executor(curSched)->try_insert(inst)) {
      sched = true;
    } else {
      curSched++;
      curSched %= this->ctrl_st_m->numExecutors();
    }
  }
  SCMULATE_INFOMSG_IF(5, sched, "Scheduling to CUMEM %d", curSched);
  SCMULATE_INFOMSG_IF(5, !sched, "Could not find a free unit");

  return sched;
}

bool scm::fetch_decode_module::gemAttemptAssignExecuteInstruction(scm::instruction_state_pair *inst)
{
  // for now, this will invoke a call to the SU
  return(owner->pushFromFD(inst));
}

bool scm::fetch_decode_module::gemAttemptAssignCommit()
{
  return(owner->commitFromFD());
}

bool scm::fetch_decode_module::fetchOperandsFromMem(scm::instruction_state_pair * inst)
{
  return(owner->fetchOperandsFromMem(inst));
}

void scm::fetch_decode_module::executeMemoryInstruction(scm::instruction_state_pair * inst)
{
  /////////////////////////////////////////////////////
  ///// LOGIC FOR THE LDIMM INSTRUCTION
  ///// Operand 1 is where to load the instructions
  ///// Operand 2 the inmediate value to be used
  /////////////////////////////////////////////////////
  if (inst->first->getOpcode() == LDIMM_INST.opcode) {
    // Obtain destination register
    decoded_reg_t reg1 = inst->first->getOp1().value.reg;
    unsigned char * reg1_ptr = reg1.reg_ptr;
    int32_t size_reg1_bytes = reg1.reg_size_bytes;

    int32_t i, j;

    // Obtain base address and perform copy
    uint64_t immediate_value = inst->first->getOp2().value.immediate;
    //uint64_t * dest = new uint64_t;
    //*dest = immediate_value;
    //this->owner->writeOp(&reg1, (void *) dest);
    this->owner->writeOp(&reg1, (void *) &immediate_value);
    //delete dest;
    
    // Perform actual memory assignment
    /*
    for (i = size_reg1_bytes-1, j = 0; i >= 0; --i, ++j) {
      if (j < 8) {
        unsigned char temp = immediate_value & 255;
        reg1_ptr[i] = temp;
        immediate_value >>= 8;
      } else {
        // Zero out the rest of the register
        reg1_ptr[i] = 0;
      }
    }
    */
    return;
  } 
  /////////////////////////////////////////////////////
  ///// LOGIC FOR THE LDADDR INSTRUCTION
  ///// Operand 1 is where to load the instructions
  ///// Operand 2 is the memory address either a register or immediate value. Only consider 64 bits
  /////////////////////////////////////////////////////
  /*
  if (inst->first->getOpcode() == LDADR_INST.opcode) {
    // Obtain destination register
    decoded_reg_t reg1 = inst->first->getOp1().value.reg;
    unsigned char * reg1_ptr = reg1.reg_ptr;
    int32_t size_reg1_bytes = reg1.reg_size_bytes;

    int32_t i, j;

    // Obtain base address and perform copy
    unsigned long base_addr = 0;
    if (inst->first->getOp2().type == operand_t::IMMEDIATE_VAL) {
      // Load address immediate value
      base_addr = myInstructionSlot->getOp2().value.immediate;
    } else if (myInstructionSlot->getOp2().type == operand_t::REGISTER) {
      // Load address register value
      decoded_reg_t reg2 = myInstructionSlot->getOp2().value.reg;
      unsigned char * reg2_ptr = reg2.reg_ptr;
      int32_t size_reg2_bytes = reg2.reg_size_bytes;
      for (i = size_reg2_bytes-1, j = 0; j < 8 || i >= 0; --i, ++j ) {
        unsigned long temp = reg2_ptr[i];
        temp <<= j*8;
        base_addr += temp;
      } 
    } 
    else {
      SCMULATE_ERROR(0, "Incorrect operand type");
    }
    // Perform actual memory copy
    std::memcpy(reg1_ptr, this->getAddress(base_addr), size_reg1_bytes);
    return;
  }
  */
  else {
    SCMULATE_ERROR(0, "Trying to execute unimplemented memory instruction");
  }

}