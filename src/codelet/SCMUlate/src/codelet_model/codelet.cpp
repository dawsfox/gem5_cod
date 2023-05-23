//#include "codelet.hpp"
//#include "instructions.hpp"
//#include "executor.hpp"
#include "codelet/SCMUlate/include/codelet_model/codelet.hpp"
#include "codelet/SCMUlate/include/common/instructions.hpp"
#include "codelet/SCMUlate/include/modules/executor.hpp"

namespace scm {

/*
std::map<std::string, creatorFnc> *codeletFactory::registeredCodelets;
static int codeletFactoryInit;
*/

//static std::map<std::string, codelet *> * registeredCodelets;

l2_memory_t 
  codelet::getAddress(uint64_t addr) {
   return this->getExecutor()->get_mem_interface()->getAddress(addr);
}
l2_memory_t 
  codelet::getAddress(l2_memory_t addr) {
   return this->getExecutor()->get_mem_interface()->getAddress(addr);
}

/* 
void
  registerCodelet(std::string codName, codelet * codPtr) {
    if (!registeredCodelets) {
      registeredCodelets = new std::map<std::string, codelet *>();
    }
    SCMULATE_INFOMSG(3, "Registering codelet %s", codName.c_str());
    (*registeredCodelets)[codName] = codPtr;
}

codelet *
  getRegisteredCodelet(std::string codName) {
    SCMULATE_INFOMSG(3, "Getting pointer for codelet %s", codName.c_str());
    codelet * fetchedCod = (*registeredCodelets)[codName];
    if (!fetchedCod) {
      SCMULATE_INFOMSG(3, "Dumping registeredCodelets map data...");
      for(auto it = registeredCodelets->cbegin(); it != registeredCodelets->cend(); ++it)
      {
        //std::cout << it->first << " " << it->second.first << " " << it->second.second << "\n";
        SCMULATE_INFOMSG(3, "Codelet %s at %p", it->first.c_str(), it->second);
      }
      SCMULATE_ERROR(0, "Failed to find registered codelet");;
    }
    SCMULATE_INFOMSG(3, "Pointer for codelet found: %p", fetchedCod);
    return((*registeredCodelets)[codName]);
}
*/

/*
codelet* 
  codeletFactory::createCodelet(std::string name, codelet_params usedParams) {
    codelet * result;
    // Look for the codelet in the map
    auto found = registeredCodelets->find(name);
    if ( found == registeredCodelets->end()) {
      // If not found, we display error, and then return null.
      SCMULATE_ERROR(0, "Trying to create a codelet that has not been implemented");
      return nullptr;
    } else {
      // If found we call the creator function and then 
      SCMULATE_INFOMSG(5, "Creating Codelet %s", name.c_str());
      creatorFnc creator = found->second;
      result = creator(usedParams); // It is a function pointer
      return result;
    }
};

void 
  codeletFactory::registerCreator(std::string name, creatorFnc creator ) {
    if (codeletFactoryInit++ == 0) registeredCodelets = new std::map<std::string, creatorFnc>();
    SCMULATE_INFOMSG(3, "Registering codelet %s", name.c_str());
    (*registeredCodelets)[name] = creator;
  }
 */
}