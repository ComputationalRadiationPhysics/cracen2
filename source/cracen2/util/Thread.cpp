#include "cracen2/util/Thread.hpp"

namespace cracen2 {

namespace util {

//explicit instanciations of extern templates must be in the corresponding namespace
template class Thread<ThreadDeletionPolicy::join>;
template class Thread<ThreadDeletionPolicy::detatch>;

}

}
