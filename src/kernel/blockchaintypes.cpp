#include <sstream>

#include "blockchain.h"
#include "crypto.h"

CryptoKernel::Blockchain::output::output(const Json::Value& jsonOutput) {
    try {
        value = jsonOutput["value"].asUInt64();
        nonce = jsonOutput["nonce"].asUInt64();
        data = jsonOutput["data"];
    } catch(const Json::Exception& e) {
        throw InvalidElementException("Output JSON is malformed");
    }

    checkRep();

    id = calculateId();
}

CryptoKernel::Blockchain::output::output(const uint64_t value, const uint64_t nonce, const Json::Value& data) {
    this->value = value;
    this->nonce = nonce;
    this->data = data;

    checkRep();

    id = calculateId();
}

void CryptoKernel::Blockchain::output::checkRep() {
    if(value < 1) {
        throw InvalidElementException("Output value cannot be less than 1");
    }

    if(data["contract"].empty() && !data["publicKey"].empty()) {
        CryptoKernel::Crypto crypto;
        try {
            if(!crypto.setPublicKey(data["publicKey"].asString())) {
                throw InvalidElementException("Public key is invalid");
            }
        } catch(const Json::Exception& e) {
            throw InvalidElementException("Output JSON is malformed");
        }
    }
}

uint64_t CryptoKernel::Blockchain::output::getValue() const {
    return value;
}

uint64_t CryptoKernel::Blockchain::output::getNonce() const {
    return nonce;
}

Json::Value CryptoKernel::Blockchain::output::getData() const {
    return data;
}

CryptoKernel::BigNum CryptoKernel::Blockchain::output::calculateId() {
    std::stringstream buffer;
    buffer << value << nonce << CryptoKernel::Storage::toString(data, false);

    CryptoKernel::Crypto crypto;
    return CryptoKernel::BigNum(crypto.sha256(buffer.str()));
}

CryptoKernel::BigNum CryptoKernel::Blockchain::output::getId() const {
    return id;
}

Json::Value CryptoKernel::Blockchain::output::toJson() const {
    Json::Value returning;

    returning["value"] = static_cast<unsigned long long int>(value);
    returning["nonce"] = static_cast<unsigned long long int>(nonce);
    returning["data"] = data;

    return returning;
}

bool CryptoKernel::Blockchain::output::operator<(const output& rhs) const {
    return getId() < rhs.getId();
}

CryptoKernel::Blockchain::dbOutput::dbOutput(const Json::Value& jsonOutput) : output(jsonOutput) {
    try {
        creationTx = CryptoKernel::BigNum(jsonOutput["creationTx"].asString());
    } catch(const Json::Exception& e) {
        throw InvalidElementException("Output JSON is malformed");
    }
}

CryptoKernel::Blockchain::dbOutput::dbOutput(const output& compactOutput, const BigNum& creationTx) : output(compactOutput.getValue(), compactOutput.getNonce(), compactOutput.getData()) {
    this->creationTx = creationTx;
}

Json::Value CryptoKernel::Blockchain::dbOutput::toJson() const {
    Json::Value returning = this->output::toJson();

    returning["creationTx"] = creationTx.toString();
    returning["id"] = getId().toString();

    return returning;
}

CryptoKernel::Blockchain::input::input(const Json::Value& inputJson) {
    try {
        data = inputJson["data"];
        outputId = CryptoKernel::BigNum(inputJson["outputId"].asString());
    } catch(const Json::Exception& e) {
        throw InvalidElementException("Input JSON is malformed");
    }

    checkRep();

    id = calculateId();
}

CryptoKernel::Blockchain::input::input(const BigNum& outputId, const Json::Value& data) {
    this->data = data;
    this->outputId = outputId;

    checkRep();

    id = calculateId();
}

void CryptoKernel::Blockchain::input::checkRep() {

}

bool CryptoKernel::Blockchain::input::operator<(const input& rhs) const {
    return getId() < rhs.getId();
}

Json::Value CryptoKernel::Blockchain::input::toJson() const {
    Json::Value returning;

    returning["outputId"] = outputId.toString();
    returning["data"] = data;

    return returning;
}

 Json::Value CryptoKernel::Blockchain::input::getData() const {
    return data;
}

CryptoKernel::BigNum CryptoKernel::Blockchain::input::getOutputId() const {
    return outputId;
}

CryptoKernel::BigNum CryptoKernel::Blockchain::input::getId() const {
    return id;
}

CryptoKernel::BigNum CryptoKernel::Blockchain::input::calculateId() {
    std::stringstream buffer;
    buffer << outputId.toString() << CryptoKernel::Storage::toString(data, false);

    CryptoKernel::Crypto crypto;
    return CryptoKernel::BigNum(crypto.sha256(buffer.str()));
}

CryptoKernel::Blockchain::dbInput::dbInput(const Json::Value& inputJson) : input(inputJson) {

}

CryptoKernel::Blockchain::dbInput::dbInput(const input& compactInput) : input(compactInput.getOutputId(), compactInput.getData()) {

}

Json::Value CryptoKernel::Blockchain::dbInput::toJson() const {
    return this->input::toJson();
}

CryptoKernel::Blockchain::transaction::transaction(const std::set<input>& inputs, const std::set<output>& outputs, const uint64_t timestamp, const bool coinbaseTx) {
    this->inputs = inputs;
    this->outputs = outputs;
    this->timestamp = timestamp;

    checkRep(coinbaseTx);

    id = calculateId();
}

CryptoKernel::Blockchain::transaction::transaction(const Json::Value& jsonTransaction, const bool coinbaseTx) {
    for(const Json::Value& inp : jsonTransaction["inputs"]) {
        inputs.insert(CryptoKernel::Blockchain::input(inp));
    }

    for(const Json::Value& out : jsonTransaction["outputs"]) {
        outputs.insert(CryptoKernel::Blockchain::output(out));
    }

    try {
        timestamp = jsonTransaction["timestamp"].asUInt64();
    } catch(const Json::Exception& e) {
        throw InvalidElementException("Transaction JSON is malformed");
    }

    checkRep(coinbaseTx);

    id = calculateId();
}

void CryptoKernel::Blockchain::transaction::checkRep(const bool coinbaseTx) {
    // Check for transaction size
    if(CryptoKernel::Storage::toString(toJson()).size() > 100 * 1024) {
        throw InvalidElementException("Transaction is too large");
    }

    if(outputs.size() < 1) {
        throw InvalidElementException("Transaction has no outputs");
    }

    if(coinbaseTx && inputs.size() > 0) {
        throw InvalidElementException("Coinbase transaction must have no inputs");
    }

    if(!coinbaseTx && inputs.size() < 1) {
        throw InvalidElementException("Transaction has no inputs");
    }

    std::set<BigNum> outputIds;

    for(const input& inp : inputs) {
        outputIds.insert(inp.getOutputId());
    }

    for(const output& out : outputs) {
        outputIds.insert(out.getId());
    }

    if(outputIds.size() != outputs.size() + inputs.size()) {
        throw InvalidElementException("Output IDs are not unique in transaction");
    }
}

CryptoKernel::BigNum CryptoKernel::Blockchain::transaction::calculateId() {
    std::stringstream buffer;

    for(const input& inp : inputs) {
        buffer << inp.getId().toString();
    }

    for(const output& out : outputs) {
        buffer << out.getId().toString();
    }

    buffer << timestamp;

    CryptoKernel::Crypto crypto;
    return CryptoKernel::BigNum(crypto.sha256(buffer.str()));
}

bool CryptoKernel::Blockchain::transaction::operator<(const transaction& rhs) const {
    return getId() < rhs.getId();
}

CryptoKernel::BigNum CryptoKernel::Blockchain::transaction::getId() const {
    return id;
}

CryptoKernel::BigNum CryptoKernel::Blockchain::transaction::getOutputSetId() const {
    return getOutputSetId(outputs);
}

CryptoKernel::BigNum CryptoKernel::Blockchain::transaction::getOutputSetId(const std::set<output>& outputs) {
    std::stringstream buffer;

    for(const output& out : outputs) {
        buffer << out.getId().toString();
    }

    CryptoKernel::Crypto crypto;
    return CryptoKernel::BigNum(crypto.sha256(buffer.str()));
}

uint64_t CryptoKernel::Blockchain::transaction::getTimestamp() const {
    return timestamp;
}

std::set<CryptoKernel::Blockchain::input> CryptoKernel::Blockchain::transaction::getInputs() const {
    return inputs;
}

std::set<CryptoKernel::Blockchain::output> CryptoKernel::Blockchain::transaction::getOutputs() const {
    return outputs;
}

Json::Value CryptoKernel::Blockchain::transaction::toJson() const {
    Json::Value returning;

    returning["timestamp"] = static_cast<unsigned long long int>(timestamp);

    for(const input& inp : inputs) {
        returning["inputs"].append(inp.toJson());
    }

    for(const output& out : outputs) {
        returning["outputs"].append(out.toJson());
    }

    return returning;
}

CryptoKernel::Blockchain::dbTransaction::dbTransaction(const Json::Value& jsonTransaction) {
    try {
        this->confirmingBlock = CryptoKernel::BigNum(jsonTransaction["confirmingBlock"].asString());
        this->coinbaseTx = jsonTransaction["coinbaseTx"].asBool();

        for(const Json::Value& inp : jsonTransaction["inputs"]) {
            inputs.insert(CryptoKernel::BigNum(inp.asString()));
        }

        for(const Json::Value& out : jsonTransaction["outputs"]) {
            outputs.insert(CryptoKernel::BigNum(out.asString()));
        }

        timestamp = jsonTransaction["timestamp"].asUInt64();
    } catch(const Json::Exception& e) {
        throw InvalidElementException("Transaction JSON is malformed");
    }

    checkRep();

    id = calculateId();
}

CryptoKernel::Blockchain::dbTransaction::dbTransaction(const transaction& compactTransaction, const BigNum& confirmingBlock, const bool coinbaseTx) {
    this->confirmingBlock = confirmingBlock;
    this->coinbaseTx = coinbaseTx;

    timestamp = compactTransaction.getTimestamp();

    for(const input& inp : compactTransaction.getInputs()) {
        inputs.insert(inp.getId());
    }

    for(const output& out : compactTransaction.getOutputs()) {
        outputs.insert(out.getId());
    }

    checkRep();

    id = calculateId();
}

CryptoKernel::BigNum CryptoKernel::Blockchain::dbTransaction::calculateId() {
    std::stringstream buffer;

    for(const BigNum& inp : inputs) {
        buffer << inp.toString();
    }

    for(const BigNum& out : outputs) {
        buffer << out.toString();
    }

    buffer << timestamp;

    CryptoKernel::Crypto crypto;
    return CryptoKernel::BigNum(crypto.sha256(buffer.str()));
}

void CryptoKernel::Blockchain::dbTransaction::checkRep () {
    if(outputs.size() < 1) {
        throw InvalidElementException("Transaction has no outputs");
    }

    if(coinbaseTx && inputs.size() > 0) {
        throw InvalidElementException("Coinbase transaction must have no inputs");
    }

    if(!coinbaseTx && inputs.size() < 1) {
        throw InvalidElementException("Transaction has no inputs");
    }
}

Json::Value CryptoKernel::Blockchain::dbTransaction::toJson() const {
    Json::Value returning;

    for(const BigNum& inp : inputs) {
        returning["inputs"].append(inp.toString());
    }

    for(const BigNum& out : outputs) {
        returning["outputs"].append(out.toString());
    }

    returning["confirmingBlock"] = confirmingBlock.toString();
    returning["coinbaseTx"] = coinbaseTx;
    returning["timestamp"] = static_cast<unsigned long long int>(timestamp);

    return returning;
}

CryptoKernel::BigNum CryptoKernel::Blockchain::dbTransaction::getId() const {
    return id;
}

uint64_t CryptoKernel::Blockchain::dbTransaction::getTimestamp() const {
    return timestamp;
}

bool CryptoKernel::Blockchain::dbTransaction::isCoinbaseTx() const {
    return coinbaseTx;
}

std::set<CryptoKernel::BigNum> CryptoKernel::Blockchain::dbTransaction::getInputs() const {
    return inputs;
}

std::set<CryptoKernel::BigNum> CryptoKernel::Blockchain::dbTransaction::getOutputs() const {
    return outputs;
}

CryptoKernel::Blockchain::block::block(const std::set<transaction>& transactions, const transaction& coinbaseTx, const BigNum& previousBlockId, const uint64_t timestamp, const Json::Value& consensusData, const uint64_t height)
: coinbaseTx(coinbaseTx.getInputs(), coinbaseTx.getOutputs(), coinbaseTx.getTimestamp(), true) {
    this->transactions = transactions;
    this->previousBlockId = previousBlockId;
    this->timestamp = timestamp;
    this->consensusData = consensusData;
    this->height = height;

    checkRep();

    id = calculateId();
}

CryptoKernel::Blockchain::block::block(const Json::Value& jsonBlock)
: coinbaseTx(jsonBlock["coinbaseTx"], true) {
    try {
        timestamp = jsonBlock["timestamp"].asUInt64();
        previousBlockId = CryptoKernel::BigNum(jsonBlock["previousBlockId"].asString());
        consensusData = jsonBlock["consensusData"];

        for(const Json::Value& tx : jsonBlock["transactions"]) {
            transactions.insert(CryptoKernel::Blockchain::transaction(tx));
        }
    } catch(const Json::Exception& e) {
        throw InvalidElementException("Block JSON is malformed");
    }

    try {
        height = jsonBlock["height"].asUInt64();
    } catch(const Json::Exception& e) {
        height = 0;
    }

    checkRep();

    id = calculateId();
}

void CryptoKernel::Blockchain::block::setConsensusData(const Json::Value& data) {
    consensusData = data;
}

CryptoKernel::BigNum CryptoKernel::Blockchain::block::calculateId() {
    std::stringstream buffer;

    for(const transaction& tx : transactions) {
        buffer << tx.getId().toString();
    }

    buffer << coinbaseTx.getId().toString() << previousBlockId.toString() << timestamp;

    CryptoKernel::Crypto crypto;
    return CryptoKernel::BigNum(crypto.sha256(buffer.str()));
}

void CryptoKernel::Blockchain::block::checkRep() {
    // Check for block size
    if(CryptoKernel::Storage::toString(toJson()).size() > 4 * 1024 * 1024) {
        throw InvalidElementException("Block is too large");
    }

    // Check for input/output conflicts
    unsigned int totalPuts = 0;
    unsigned int totalInputs = 0;
    std::set<BigNum> outputIds;
    std::set<BigNum> inputIds;
    for(const transaction& tx : transactions) {
        const std::set<input> inputs = tx.getInputs();
        const std::set<output> outputs = tx.getOutputs();

        for(const input& inp : inputs) {
            totalPuts++;
            totalInputs++;
            outputIds.insert(inp.getOutputId());
            inputIds.insert(inp.getId());
        }

        for(const output& out : outputs) {
            totalPuts++;
            outputIds.insert(out.getId());
        }
    }

    // Coinbase tx should have no inputs, others should have at least 1
    const std::set<input> inputs = coinbaseTx.getInputs();
    const std::set<output> outputs = coinbaseTx.getOutputs();

    for(const output& out : outputs) {
        totalPuts++;
        outputIds.insert(out.getId());
    }

    if(totalPuts != outputIds.size()) {
        throw InvalidElementException("Block contains duplicate outputs");
    }

    if(totalInputs != inputIds.size()) {
        throw InvalidElementException("Block contains duplicate inputs");
    }
}

Json::Value CryptoKernel::Blockchain::block::toJson() const {
    Json::Value returning;
    returning["coinbaseTx"] = coinbaseTx.toJson();
    returning["previousBlockId"] = previousBlockId.toString();
    returning["timestamp"] = static_cast<unsigned long long int>(timestamp);
    returning["consensusData"] = consensusData;
    returning["height"] = static_cast<unsigned long long int>(height);

    for(const transaction& tx : transactions) {
        returning["transactions"].append(tx.toJson());
    }

    return returning;
}

std::set<CryptoKernel::Blockchain::transaction> CryptoKernel::Blockchain::block::getTransactions() const {
    return transactions;
}

CryptoKernel::Blockchain::transaction CryptoKernel::Blockchain::block::getCoinbaseTx() const {
    return coinbaseTx;
}

CryptoKernel::BigNum CryptoKernel::Blockchain::block::getPreviousBlockId() const {
    return previousBlockId;
}

uint64_t CryptoKernel::Blockchain::block::getTimestamp() const {
    return timestamp;
}

Json::Value CryptoKernel::Blockchain::block::getConsensusData() const {
    return consensusData;
}

CryptoKernel::BigNum CryptoKernel::Blockchain::block::getId() const {
    return id;
}

uint64_t CryptoKernel::Blockchain::block::getHeight() const {
    return height;
}

CryptoKernel::Blockchain::dbBlock::dbBlock(const Json::Value& jsonBlock) {
    try {
        coinbaseTx = CryptoKernel::BigNum(jsonBlock["coinbaseTx"].asString());
        previousBlockId = CryptoKernel::BigNum(jsonBlock["previousBlockId"].asString());
        timestamp = jsonBlock["timestamp"].asUInt64();
        height = jsonBlock["height"].asUInt64();
        consensusData = jsonBlock["consensusData"];

        for(const Json::Value& tx : jsonBlock["transactions"]) {
            transactions.insert(CryptoKernel::BigNum(tx.asString()));
        }
    } catch(const Json::Exception& e) {
        throw InvalidElementException("Block JSON is malformed");
    }

    checkRep();

    id = calculateId();
}

CryptoKernel::Blockchain::dbBlock::dbBlock(const block& compactBlock) {
    coinbaseTx = compactBlock.getCoinbaseTx().getId();
    previousBlockId = compactBlock.getPreviousBlockId();
    timestamp = compactBlock.getTimestamp();
    consensusData = compactBlock.getConsensusData();
    this->height = compactBlock.getHeight();

    for(const transaction& tx : compactBlock.getTransactions()) {
        transactions.insert(tx.getId());
    }

    checkRep();

    id = calculateId();
}

CryptoKernel::Blockchain::dbBlock::dbBlock(const block& compactBlock, const uint64_t height) {
    coinbaseTx = compactBlock.getCoinbaseTx().getId();
    previousBlockId = compactBlock.getPreviousBlockId();
    timestamp = compactBlock.getTimestamp();
    consensusData = compactBlock.getConsensusData();
    this->height = height;

    for(const transaction& tx : compactBlock.getTransactions()) {
        transactions.insert(tx.getId());
    }

    checkRep();

    id = calculateId();
}

void CryptoKernel::Blockchain::dbBlock::checkRep() {

}

CryptoKernel::BigNum CryptoKernel::Blockchain::dbBlock::calculateId() {
    std::stringstream buffer;

    for(const BigNum& tx : transactions) {
        buffer << tx.toString();
    }

    buffer << coinbaseTx.toString() << previousBlockId.toString() << timestamp;

    CryptoKernel::Crypto crypto;
    return CryptoKernel::BigNum(crypto.sha256(buffer.str()));
}

Json::Value CryptoKernel::Blockchain::dbBlock::toJson() const {
    Json::Value returning;

    returning["coinbaseTx"] = coinbaseTx.toString();
    returning["previousBlockId"] = previousBlockId.toString();
    returning["timestamp"] = static_cast<unsigned long long int>(timestamp);
    returning["consensusData"] = consensusData;
    returning["height"] = static_cast<unsigned long long int>(height);

    for(const BigNum& tx : transactions) {
        returning["transactions"].append(tx.toString());
    }

    return returning;
}

std::set<CryptoKernel::BigNum> CryptoKernel::Blockchain::dbBlock::getTransactions() const {
    return transactions;
}

CryptoKernel::BigNum CryptoKernel::Blockchain::dbBlock::getCoinbaseTx() const {
    return coinbaseTx;
}

CryptoKernel::BigNum CryptoKernel::Blockchain::dbBlock::getPreviousBlockId() const {
    return previousBlockId;
}

uint64_t CryptoKernel::Blockchain::dbBlock::getTimestamp() const {
    return timestamp;
}

uint64_t CryptoKernel::Blockchain::dbBlock::getHeight() const {
    return height;
}

Json::Value CryptoKernel::Blockchain::dbBlock::getConsensusData() const {
    return consensusData;
}

CryptoKernel::BigNum CryptoKernel::Blockchain::dbBlock::getId() const {
    return id;
}
