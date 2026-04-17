#pragma once

#include "ac6dm/contracts/exchange_contracts.hpp"

#include <vector>

namespace ac6dm::exchange {

ac6dm::contracts::ExchangeFormatContract describeMinimalExchangeFormat(
    ac6dm::contracts::ExchangeAssetKind assetKind);

std::vector<ac6dm::contracts::FutureActionContract> buildN2AFutureActionContracts();

}  // namespace ac6dm::exchange
