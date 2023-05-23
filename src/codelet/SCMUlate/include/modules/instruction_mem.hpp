#ifndef __INST_MEM__
#define __INST_MEM__

//#include "SCMUlate_tools.hpp"
//#include "instructions.hpp"
//#include "register.hpp"
#include "codelet/SCMUlate/include/common/SCMUlate_tools.hpp"
#include "codelet/SCMUlate/include/common/instructions.hpp"
#include "codelet/SCMUlate/include/modules/register.hpp"
#include <vector>
#include <map>
#include <string>
#include <fstream>
#include <iostream>

namespace gem5 {
  class SU; //forward declaration so we can have a pointer; instruction mem needs to be able
  // to access the codelet mapping so it can set fire functions. Can't include the file because
  // SU.hh includes this file -- circular dependency
}

namespace scm {

  /* This module corresponds to the instruction memory, it is in charge 
   * of reading the file that contains the program, or read the program 
   * directly from the user input. It uses the instructions class to 
   * identify if the readed line is an instruction, otherwise it will try
   * to map it to a label, if that does not work it will ignore the line
   * and raise a warning. Additionally, it is possible to fetch any instruction
   * by passing the corresponding address
   *
   */
  class inst_mem_module {
    private: 
      std::vector<decoded_instruction_t*> memory;
      std::map<std::string, int> labels;
      reg_file_module * reg_file_m;
      gem5::SU * owner;

      /* This flag checks if the file that is received is a valid flag
       * otherwise it should set it to stop running the machine
       */
      bool is_valid; 

      /* This method parses the file that is sent containing the program
       * and load all the insturctions and labels into the instruction memory
       * If the program is invalid it will return false. If the program is 
       * valid it will return true
       */
      //bool loader(char * filename);
      bool loader(const char * filename);

    public:
      inst_mem_module() = delete;
      //inst_mem_module(char * filename, reg_file_module * const reg_file_m);
      inst_mem_module(const char * filename, reg_file_module * const reg_file_m, gem5::SU * owner);
  
      /* This method allows to fetch an instruction from the instruction 
       * memory by passing the address (PC)
       */
      inline decoded_instruction_t* fetch(int address) { return memory[address]; };

      /** \brief translate label to memory
       *
       *
       */
      inline int getMemoryLabel(std::string label) { 
        auto it = labels.find(label); 
        if (it != labels.end())
          return it->second;
        else
          return -1;
      }

      bool isValid() { return this->is_valid; }

      inline uint32_t getMemSize() { return this->memory.size(); }
      inline reg_file_module* getRegisterFileModule() {return this->reg_file_m; }

      gem5::SU * getOwner() { return this->owner; }

      /* This method allows to dump the content of the instruction 
       * memory for debugging
       */
      void dumpMemory();
      ~inst_mem_module();
  };
}

#endif
