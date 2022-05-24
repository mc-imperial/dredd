#ifndef DREDD_NEW_MUTATE_FRONTEND_ACTION_FACTORY_H
#define DREDD_NEW_MUTATE_FRONTEND_ACTION_FACTORY_H

#include <memory>
#include <random>
#include <string>

#include "clang/Tooling/Tooling.h"

namespace dredd {

std::unique_ptr<clang::tooling::FrontendActionFactory> NewMutateFrontendActionFactory(size_t num_mutations, const std::string& output_filename, std::mt19937& generator);

}

#endif // DREDD_NEW_MUTATE_FRONTEND_ACTION_FACTORY_H
