#include "cracen2/util/Thread.hpp"

namespace cracen2 {

namespace util {

//explicit instanciations of extern templates must be in the corresponding namespace
template struct Thread<ThreadDeletionPolicy::join>;
template struct Thread<ThreadDeletionPolicy::detatch>;

}

}
