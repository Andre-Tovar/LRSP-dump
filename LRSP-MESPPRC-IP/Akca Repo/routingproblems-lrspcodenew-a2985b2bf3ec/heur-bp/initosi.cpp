//$Id:$

#include <OsiSolverInterface.hpp>
#if defined(LPSOLVER_CPLEX)
#include <OsiCpxSolverInterface.hpp>
#elif defined(LPSOLVER_CLP)
#include <OsiClpSolverInterface.hpp>
#else
#error "No appropriate OSI_<LPSOLVER> defined in Makefile"
#endif

extern "C" void InitLP(void);


OsiSolverInterface *osilp = NULL;
void InitLP()
{
#if defined(LPSOLVER_CPLEX)
  osilp = new OsiCpxSolverInterface();
#elif defined(LPSOLVER_CLP)
  osilp = new OsiClpSolverInterface();
#else
#error "OSI_<LPSOLVER> not #defined"
#endif

 osilp->setHintParam(OsiDoReducePrint);
 osilp->messageHandler()->setLogLevel(0); 

}
