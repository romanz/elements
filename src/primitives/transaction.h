// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_PRIMITIVES_TRANSACTION_H
#define BITCOIN_PRIMITIVES_TRANSACTION_H

#include "amount.h"
#include "script/script.h"
#include "serialize.h"
#include "uint256.h"

static const int SERIALIZE_TRANSACTION_NO_WITNESS = 0x40000000;
static const int SERIALIZE_BITCOIN_BLOCK_OR_TX = 0x20000000;

static const int WITNESS_SCALE_FACTOR = 4;

static const CFeeRate withdrawLockTxFee = CFeeRate(5460);

class CTxOutAsset
{
public:
    static const size_t nAssetTagSize = 33;

    std::vector<unsigned char> vchAssetTag;
    std::vector<unsigned char> vchSurjectionproof;

    CTxOutAsset()
    {
        vchAssetTag.reserve(nAssetTagSize);
        SetNull();
    }

    CTxOutAsset(const uint256& assetID)
    {
        SetToAssetID(assetID);
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        if ((nVersion & SERIALIZE_BITCOIN_BLOCK_OR_TX) || IsInBitcoinTransaction()) {
            if (ser_action.ForRead()) {
                vchAssetTag.resize(1);
                vchAssetTag[0] = 0;
                vchSurjectionproof.clear();
            }
        } else {
            vchAssetTag.resize(nAssetTagSize);
            READWRITE(REF(CFlatData(&vchAssetTag[0], &vchAssetTag[nAssetTagSize])));
            // The surjection proof is serialized as part of the witness data
        }
    }

    bool IsNull() const
    {
        return vchAssetTag.empty() || vchAssetTag[0]==0xff;
    }

    void SetNull();

    bool IsAssetID() const
    {
        return vchAssetTag.size()==nAssetTagSize && vchAssetTag[0]==1;
    }
    bool GetAssetID(uint256& assetID) const;

    bool IsAssetCommitment() const
    {
        return vchAssetTag.size()==nAssetTagSize && (vchAssetTag[0]==10 || vchAssetTag[0]==11);
    }

    bool IsAssetGeneration() const
    {
        return vchAssetTag.size()==nAssetTagSize && vchAssetTag[0]==12;
    }

    void SetAsAssetGeneration()
    {
        vchAssetTag[0] = 12;
    }

    friend bool operator==(const CTxOutAsset& a, const CTxOutAsset& b)
    {
        return (a.vchAssetTag        == b.vchAssetTag &&
                a.vchSurjectionproof == b.vchSurjectionproof);
    }

    friend bool operator!=(const CTxOutAsset& a, const CTxOutAsset& b)
    {
        return !(a == b);
    }

private:
    bool IsInBitcoinTransaction() const { return vchAssetTag[0] == 0; }
    void SetToAssetID(const uint256& assetID);
};

class CTxOutValue
{
public:
    static const size_t nExplicitSize = 9;
    static const size_t nCommittedSize = 33;

    std::vector<unsigned char> vchCommitment;
    std::vector<unsigned char> vchRangeproof;
    std::vector<unsigned char> vchNonceCommitment;

    CTxOutValue() { SetNull(); }
    CTxOutValue(CAmount);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        if ((nVersion & SERIALIZE_BITCOIN_BLOCK_OR_TX) || IsInBitcoinTransaction()) {
            CAmount nAmount = 0;
            if (!ser_action.ForRead())
                nAmount = GetAmount();
            READWRITE(nAmount);
            if (ser_action.ForRead())
                SetToBitcoinAmount(nAmount);
        } else {
            // We only serialize the value commitment here.
            // The ECDH key and range proof are serialized through CTxOutWitnessSerializer.
            READWRITE(vchCommitment.front());
            if (ser_action.ForRead()) {
                switch (vchCommitment.front()) {
                    case 0:
                    case 1:
                        vchCommitment.resize(nExplicitSize);
                        break;
                    // Alpha used 2 and 3 for value commitments
                    case 2:
                    case 3:
                        break;
                    case 8:
                    case 9:
                        vchCommitment.resize(nCommittedSize);
                        break;
                    default:
                        vchCommitment.resize(1);
                        return;
                }
            }
            READWRITE(REF(CFlatData(&vchCommitment[1], &vchCommitment[vchCommitment.size()])));
        }
    }

    void SetNull();
    bool IsNull() const { return vchCommitment[0] == 0xff; }

    bool IsValid() const;

    // True for both native Amounts and "Bitcoin amounts"
    bool IsAmount() const { return vchCommitment[0] == 0 || vchCommitment[0] == 1; }
    CAmount GetAmount() const;

    friend bool operator==(const CTxOutValue& a, const CTxOutValue& b)
    {
        return a.vchRangeproof == b.vchRangeproof &&
               a.vchCommitment == b.vchCommitment &&
               a.vchNonceCommitment == b.vchNonceCommitment;
    }

    friend bool operator!=(const CTxOutValue& a, const CTxOutValue& b)
    {
        return !(a == b);
    }

private: // "Bitcoin amounts" can only be set by deserializing with SERIALIZE_BITCOIN_BLOCK_OR_TX
    void SetToBitcoinAmount(const CAmount nAmount);
    bool IsInBitcoinTransaction() const { return vchCommitment[0] == 0; }
    void SetToAmount(const CAmount nAmount);
};

/** An output of a transaction.  It contains the public key that the next input
 * must be able to sign with to claim it.
 */
class CTxOut
{
public:
    CTxOutAsset nAsset;
    CTxOutValue nValue;
    CScript scriptPubKey;

    // FIXME: Inventory the places this constructor is called, and make sure
    //        that `nAsset` is being set appropriately.
    CTxOut()
    {
        SetNull();
    }

    // FIXME: Add `const CTxOutAsset& nAssetIn` as first parameter. This will
    //        (rightfully) break all code that calls this constructor, which
    //        will need to be fixed to be asset aware.
    CTxOut(const CTxOutAsset& nAssetIn, const CTxOutValue& nValueIn, CScript scriptPubKeyIn);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(nAsset);
        READWRITE(nValue);
        READWRITE(*(CScriptBase*)(&scriptPubKey));
    }

    void SetNull()
    {
        nAsset = CTxOutAsset();
        nValue = CTxOutValue();
        scriptPubKey.clear();
    }

    bool IsNull() const
    {
        return nAsset.IsNull() && nValue.IsNull() && scriptPubKey.empty();
    }

    CAmount GetDustThreshold(const CFeeRate &minRelayTxFee) const
    {
        // "Dust" is defined in terms of CTransaction::minRelayTxFee,
        // which has units satoshis-per-kilobyte.
        // If you'd pay more than 1/3 in fees
        // to spend something, then we consider it dust.
        // A typical spendable non-segwit txout is 34 bytes big, and will
        // need a CTxIn of at least 148 bytes to spend:
        // so dust is a spendable txout less than
        // 546*minRelayTxFee/1000 (in satoshis).
        // A typical spendable segwit txout is 31 bytes big, and will
        // need a CTxIn of at least 67 bytes to spend:
        // so dust is a spendable txout less than
        // 294*minRelayTxFee/1000 (in satoshis).
        if (scriptPubKey.IsUnspendable())
            return 0;

        size_t nSize = GetSerializeSize(SER_DISK, 0);
        int witnessversion = 0;
        std::vector<unsigned char> witnessprogram;

        if (scriptPubKey.IsWitnessProgram(witnessversion, witnessprogram)) {
            // sum the sizes of the parts of a transaction input
            // with 75% segwit discount applied to the script size.
            nSize += (32 + 4 + 1 + (107 / WITNESS_SCALE_FACTOR) + 4);
        } else {
            nSize += (32 + 4 + 1 + 107 + 4); // the 148 mentioned above
        }

        return 3 * minRelayTxFee.GetFee(nSize);
    }

    bool IsDust(const CFeeRate &minRelayTxFee) const
    {
        if (!nValue.IsAmount())
            return false; // FIXME
        uint256 assetid;
        if (!nAsset.GetAssetID(assetid) || assetid != BITCOINID)
            return false;
        //Withdrawlocks are evaluated at a higher, static feerate
        //to ensure peg-outs are IsStandard on mainchain
        if (scriptPubKey.IsWithdrawLock() && nValue.GetAmount() < GetDustThreshold(withdrawLockTxFee))
            return true;
        return (nValue.GetAmount() < GetDustThreshold(minRelayTxFee));
    }

    bool IsFee() const
    {
        uint256 assetid;
        if (scriptPubKey == CScript() && nValue.IsAmount() && nAsset.IsAssetID())
            return true;
        return false;
    }

    friend bool operator==(const CTxOut& a, const CTxOut& b)
    {
        return (a.nValue       == b.nValue &&
                a.nValue       == b.nValue &&
                a.scriptPubKey == b.scriptPubKey);
    }

    friend bool operator!=(const CTxOut& a, const CTxOut& b)
    {
        return !(a == b);
    }

    std::string ToString() const;
};

/** An outpoint - a combination of a transaction hash and an index n into its vout */
class COutPoint
{
public:
    uint256 hash;
    uint32_t n;

    /* If this flag set, the CTxIn including this COutPoint has a
     * CAssetIssuance object. */
    static const uint32_t OUTPOINT_ISSUANCE_FLAG = (1 << 31);

    /* The inverse of the combination of the preceeding flags. Used to
     * extract the original meaning of `n` as the index into the
     * transaction's output array. */
    static const uint32_t OUTPOINT_INDEX_MASK = 0x7fffffff;

    COutPoint() { SetNull(); }
    COutPoint(uint256 hashIn, uint32_t nIn) { hash = hashIn; n = nIn; }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(hash);
        READWRITE(n);
    }

    void SetNull() { hash.SetNull(); n = (uint32_t) -1; }
    bool IsNull() const { return (hash.IsNull() && n == (uint32_t) -1); }

    friend bool operator<(const COutPoint& a, const COutPoint& b)
    {
        int cmp = a.hash.Compare(b.hash);
        return cmp < 0 || (cmp == 0 && a.n < b.n);
    }

    friend bool operator==(const COutPoint& a, const COutPoint& b)
    {
        return (a.hash == b.hash && a.n == b.n);
    }

    friend bool operator!=(const COutPoint& a, const COutPoint& b)
    {
        return !(a == b);
    }

    std::string ToString() const;
};

class CAssetIssuance
{
public:
    // == 0
    //   Indicates new asset issuance.
    // != 0
    //   This is a revelation of the blinding key for the input,
    //   which shows that the input being spent is of the reissuance
    //   capability type for the asset being inflated.
    uint256 assetBlindingNonce;

    // New asset issuance:
    //   This is a 32-byte nonce of no consensus-defined meaning,
    //   but is used as additional entropy to the asset tag calculation.
    //   This is used by higher-layer protocols for defining the
    //   Ricardian contract governing the asset.
    // Existing asset reissuance:
    //   The original asset entropy (combination of Ricardian contract
    //   and outpoint used) which was used to generate the fixed asset
    //   tag and reissuance tokens.
    uint256 hashAssetIdentifier;

    // Both explicit and blinded issuance amounts are supported
    // (see class definition for CTxOutValue for details).
    CTxOutValue nAmount;

    // If nonzero, specifies the number of asset issuance tokens to
    // generate. These tokens are made available to the outputs of the
    // generating transaction.
    CTxOutValue nInflationKeys;

public:
    CAssetIssuance()
    {
        SetNull();
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(assetBlindingNonce);
        READWRITE(hashAssetIdentifier);
        READWRITE(nAmount);
        READWRITE(nInflationKeys);
    }

    void SetNull() { nAmount.SetNull(); }
    bool IsNull() const { return nAmount.IsNull(); }

    friend bool operator==(const CAssetIssuance& a, const CAssetIssuance& b)
    {
        return a.assetBlindingNonce == b.assetBlindingNonce &&
               a.hashAssetIdentifier == b.hashAssetIdentifier &&
               a.nAmount == b.nAmount &&
               a.nInflationKeys == b.nInflationKeys;
    }

    friend bool operator!=(const CAssetIssuance& a, const CAssetIssuance& b)
    {
        return !(a == b);
    }

    std::string ToString() const;
};

/** An input of a transaction.  It contains the location of the previous
 * transaction's output that it claims and a signature that matches the
 * output's public key.
 */
class CTxIn
{
public:
    COutPoint prevout;
    CScript scriptSig;
    uint32_t nSequence;
    CAssetIssuance assetIssuance;

    /* Setting nSequence to this value for every input in a transaction
     * disables nLockTime. */
    static const uint32_t SEQUENCE_FINAL = 0xffffffff;

    /* Below flags apply in the context of BIP 68*/
    /* If this flag set, CTxIn::nSequence is NOT interpreted as a
     * relative lock-time. */
    static const uint32_t SEQUENCE_LOCKTIME_DISABLE_FLAG = (1 << 31);

    /* If CTxIn::nSequence encodes a relative lock-time and this flag
     * is set, the relative lock-time has units of 512 seconds,
     * otherwise it specifies blocks with a granularity of 1. */
    static const uint32_t SEQUENCE_LOCKTIME_TYPE_FLAG = (1 << 22);

    /* If CTxIn::nSequence encodes a relative lock-time, this mask is
     * applied to extract that lock-time from the sequence field. */
    static const uint32_t SEQUENCE_LOCKTIME_MASK = 0x0000ffff;

    /* In order to use the same number of bits to encode roughly the
     * same wall-clock duration, and because blocks are naturally
     * limited to occur every 600s on average, the minimum granularity
     * for time-based relative lock-time is fixed at 512 seconds.
     * Converting from CTxIn::nSequence to seconds is performed by
     * multiplying by 512 = 2^9, or equivalently shifting up by
     * 9 bits. */
    static const int SEQUENCE_LOCKTIME_GRANULARITY = 9;

    CTxIn()
    {
        nSequence = SEQUENCE_FINAL;
    }

    explicit CTxIn(COutPoint prevoutIn, CScript scriptSigIn=CScript(), uint32_t nSequenceIn=SEQUENCE_FINAL);
    CTxIn(uint256 hashPrevTx, uint32_t nOut, CScript scriptSigIn=CScript(), uint32_t nSequenceIn=SEQUENCE_FINAL);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        bool fHasAssetIssuance;
        COutPoint outpoint;

        if (!ser_action.ForRead()) {
            // The issuance bit can't be set as it is used to indicate
            // the presence of the asset issuance or objects. It should
            // never be set anyway as that would require a parent
            // transaction with over two billion outputs.
            assert(!(prevout.n & ~COutPoint::OUTPOINT_INDEX_MASK));
            // The assetIssuance object is used to represent both new
            // asset generation and reissuance of existing asset types.
            fHasAssetIssuance = !assetIssuance.IsNull();
            // The mode is placed in the upper bits of the outpoint's
            // index field. The IssuanceMode enum values are chosen to
            // make this as simple as a bitwise-OR.
            outpoint.hash = prevout.hash;
            outpoint.n = prevout.n & COutPoint::OUTPOINT_INDEX_MASK;
        }

        READWRITE(outpoint);

        if (ser_action.ForRead()) {
            // The presense of the asset issuance object is indicated by
            // a bit set in the outpoint index field.
            fHasAssetIssuance = !!(outpoint.n & COutPoint::OUTPOINT_ISSUANCE_FLAG);
            // The mode, if set, must be masked out of the outpoint so
            // that the in-memory index field retains its traditional
            // meaning of identifying the index into the output array
            // of the previous transaction.
            prevout.hash = outpoint.hash;
            prevout.n = outpoint.n & COutPoint::OUTPOINT_INDEX_MASK;
        }

        READWRITE(*(CScriptBase*)(&scriptSig));
        READWRITE(nSequence);

        // The asset fields are deserialized only if they are present.
        if (fHasAssetIssuance) {
            READWRITE(assetIssuance);
        } else if (ser_action.ForRead()) {
            assetIssuance.SetNull();
        }
    }

    friend bool operator==(const CTxIn& a, const CTxIn& b)
    {
        return (a.prevout       == b.prevout &&
                a.scriptSig     == b.scriptSig &&
                a.nSequence     == b.nSequence &&
                a.assetIssuance == b.assetIssuance);
    }

    friend bool operator!=(const CTxIn& a, const CTxIn& b)
    {
        return !(a == b);
    }

    std::string ToString() const;
};


class CTxOutWitnessSerializer
{
    CTxOut& ref;

public:
    CTxOutWitnessSerializer(CTxOut& ref_) : ref(ref_) {}

    ADD_SERIALIZE_METHODS;

    bool IsNull() const {
        return ref.nAsset.vchSurjectionproof.empty() && ref.nValue.vchRangeproof.empty() && ref.nValue.vchNonceCommitment.empty();
    }

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        if (!(nVersion & SERIALIZE_BITCOIN_BLOCK_OR_TX)) {
            READWRITE(ref.nAsset.vchSurjectionproof);
            READWRITE(ref.nValue.vchRangeproof);
            READWRITE(ref.nValue.vchNonceCommitment);
        }
    }

    void SetNull() {
        std::vector<unsigned char>().swap(ref.nAsset.vchSurjectionproof);
        std::vector<unsigned char>().swap(ref.nValue.vchRangeproof);
        std::vector<unsigned char>().swap(ref.nValue.vchNonceCommitment);
    }
};

class CTxInWitness
{
public:
    CScriptWitness scriptWitness;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(scriptWitness.stack);
    }

    bool IsNull() const { return scriptWitness.IsNull(); }

    CTxInWitness() { }
};

class CTxWitness
{
public:
    /** In case vtxinwit is missing, all entries are treated as if they were empty CTxInWitnesses */
    std::vector<CTxInWitness> vtxinwit;

    ADD_SERIALIZE_METHODS;

    bool IsEmpty() const { return vtxinwit.empty(); }

    bool IsNull() const
    {
        for (size_t n = 0; n < vtxinwit.size(); n++) {
            if (!vtxinwit[n].IsNull()) {
                return false;
            }
        }
        return true;
    }

    void SetNull()
    {
        vtxinwit.clear();
    }

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        for (size_t n = 0; n < vtxinwit.size(); n++) {
            READWRITE(vtxinwit[n]);
        }
        if (IsNull()) {
            /* It's illegal to encode a witness when all vtxinwit entries are empty. */
            throw std::ios_base::failure("Superfluous witness record");
        }
    }
};

struct CMutableTransaction;

/**
 * Basic transaction serialization format:
 * - int32_t nVersion
 * - std::vector<CTxIn> vin
 * - std::vector<CTxOut> vout
 * - uint32_t nLockTime
 *
 * Extended transaction serialization format:
 * - int32_t nVersion
 * - int32_t nTxFee
 * - unsigned char dummy = 0x00
 * - unsigned char flags (!= 0)
 * - std::vector<CTxIn> vin
 * - std::vector<CTxOut> vout
 * - if (flags & 1):
 *   - CTxWitness wit;
 * - if (flags & 2):
 *   - CTxOutWitness witout;
 * - uint32_t nLockTime
 */
static const CAmount TX_FEE_BITCOIN_TX_FLAG = -42;
template<typename Stream, typename Operation, typename TxType>
inline void SerializeTransaction(TxType& tx, Stream& s, Operation ser_action, int nType, int nVersion) {
    const bool fAllowWitness = !(nVersion & SERIALIZE_TRANSACTION_NO_WITNESS);
    const bool fIsBitcoinTx = (nVersion & SERIALIZE_BITCOIN_BLOCK_OR_TX);
    READWRITE(*const_cast<int32_t*>(&tx.nVersion));
    unsigned char flags = 0;
    if (ser_action.ForRead()) {
        const_cast<std::vector<CTxIn>*>(&tx.vin)->clear();
        const_cast<std::vector<CTxOut>*>(&tx.vout)->clear();
        const_cast<CTxWitness*>(&tx.wit)->SetNull();
        /* Try to read the vin. In case the dummy is there, this will be read as an empty vector. */
        READWRITE(*const_cast<std::vector<CTxIn>*>(&tx.vin));
        if (tx.vin.size() == 0 && !(nVersion & SERIALIZE_TRANSACTION_NO_WITNESS)) {
            /* We read a dummy or an empty vin. */
            READWRITE(flags);
            if (flags != 0) {
                READWRITE(*const_cast<std::vector<CTxIn>*>(&tx.vin));
                READWRITE(*const_cast<std::vector<CTxOut>*>(&tx.vout));
            }
        } else {
            /* We read a non-empty vin. Assume a normal vout follows. */
            READWRITE(*const_cast<std::vector<CTxOut>*>(&tx.vout));
        }
        if ((flags & 1) && !(nVersion & SERIALIZE_TRANSACTION_NO_WITNESS)) {
            /* The witness flag is present, and we support witnesses. */
            flags ^= 1;
            const_cast<CTxWitness*>(&tx.wit)->vtxinwit.resize(tx.vin.size());
            READWRITE(tx.wit);
        }
        if ((flags & 2) && fAllowWitness && !fIsBitcoinTx) {
            /* The witness output flag is present, and we support witnesses. */
            flags ^= 2;
            bool fHadOutputWitness = false;
            for (size_t i = 0; i < tx.vout.size(); i++) {
                CTxOutWitnessSerializer witser(REF(tx.vout[i]));
                READWRITE(witser);
                if (!witser.IsNull()) {
                    fHadOutputWitness = true;
                }
            }
            if (!fHadOutputWitness) {
                throw std::ios_base::failure("Superfluous output witness record");
            }
        }
        if (flags) {
            /* Unknown flag in the serialization */
            throw std::ios_base::failure("Unknown transaction optional data");
        }
    } else {
        // Consistency check
        assert(tx.wit.vtxinwit.size() <= tx.vin.size());
        if (!(nVersion & SERIALIZE_TRANSACTION_NO_WITNESS)) {
            /* Check whether witnesses need to be serialized. */
            if (!tx.wit.IsNull()) {
                flags |= 1;
            }
            if (!fIsBitcoinTx) {
                for (size_t i = 0; i < tx.vout.size(); i++) {
                    if (!CTxOutWitnessSerializer(*const_cast<CTxOut*>(&tx.vout[i])).IsNull()) {
                        flags |= 2;
                        break;
                    }
                }
            }
        }
        if (flags) {
            /* Use extended format in case witnesses are to be serialized. */
            std::vector<CTxIn> vinDummy;
            READWRITE(vinDummy);
            READWRITE(flags);
        }
        READWRITE(*const_cast<std::vector<CTxIn>*>(&tx.vin));
        READWRITE(*const_cast<std::vector<CTxOut>*>(&tx.vout));
        if (flags & 1) {
            const_cast<CTxWitness*>(&tx.wit)->vtxinwit.resize(tx.vin.size());
            READWRITE(tx.wit);
        }
        if (flags & 2) {
            for (size_t i = 0; i < tx.vout.size(); i++) {
                CTxOutWitnessSerializer witser(*const_cast<CTxOut*>(&tx.vout[i]));
                READWRITE(witser);
            }
        }
    }
    READWRITE(*const_cast<uint32_t*>(&tx.nLockTime));
}

/** The basic transaction that is broadcasted on the network and contained in
 * blocks.  A transaction can contain multiple inputs and outputs.
 */
class CTransaction
{
private:
    /** Memory only. */
    const uint256 hash;

public:
    // Default transaction version.
    static const int32_t CURRENT_VERSION=1;

    // Changing the default transaction version requires a two step process: first
    // adapting relay policy by bumping MAX_STANDARD_VERSION, and then later date
    // bumping the default CURRENT_VERSION at which point both CURRENT_VERSION and
    // MAX_STANDARD_VERSION will be equal.
    static const int32_t MAX_STANDARD_VERSION=2;

    // The local variables are made const to prevent unintended modification
    // without updating the cached hash value. However, CTransaction is not
    // actually immutable; deserialization and assignment are implemented,
    // and bypass the constness. This is safe, as they update the entire
    // structure, including the hash.
    const int32_t nVersion;
    const std::vector<CTxIn> vin;

    const std::vector<CTxOut> vout;
    CTxWitness wit; // Not const: can change without invalidating the txid cache
    const uint32_t nLockTime;

    /** Construct a CTransaction that qualifies as IsNull() */
    CTransaction();

    /** Convert a CMutableTransaction into a CTransaction. */
    CTransaction(const CMutableTransaction &tx);

    CTransaction& operator=(const CTransaction& tx);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        SerializeTransaction(*this, s, ser_action, nType, nVersion);
        if (ser_action.ForRead()) {
            UpdateHash();
        }
    }

    bool IsNull() const {
        return vin.empty() && vout.empty();
    }

    const uint256& GetHash() const {
        return hash;
    }

    // Compute a hash that includes both transaction and witness data
    uint256 GetWitnessHash() const;

    // Check if explicit TX fees overflow or are negative
    bool HasValidFee() const;

    // Compute the fee from the explicit fee outputs. Must call HasValidFee first
    CAmountMap GetFee() const;

    // Compute priority, given priority of inputs and (optionally) tx size
    double ComputePriority(double dPriorityInputs, unsigned int nTxSize=0) const;

    // Compute modified tx size for priority calculation (optionally given tx size)
    unsigned int CalculateModifiedSize(unsigned int nTxSize=0) const;

    bool IsCoinBase() const
    {
        return (vin.size() == 1 && vin[0].prevout.IsNull());
    }

    friend bool operator==(const CTransaction& a, const CTransaction& b)
    {
        return a.hash == b.hash;
    }

    friend bool operator!=(const CTransaction& a, const CTransaction& b)
    {
        return a.hash != b.hash;
    }

    std::string ToString() const;

    void UpdateHash() const;
};

/** A mutable version of CTransaction. */
struct CMutableTransaction
{
    int32_t nVersion;
    std::vector<CTxIn> vin;
    std::vector<CTxOut> vout;
    CTxWitness wit;
    uint32_t nLockTime;

    CMutableTransaction();
    CMutableTransaction(const CTransaction& tx);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        SerializeTransaction(*this, s, ser_action, nType, nVersion);
    }

    /** Compute the hash of this CMutableTransaction. This is computed on the
     * fly, as opposed to GetHash() in CTransaction, which uses a cached result.
     */
    uint256 GetHash() const;
};

/** Compute the weight of a transaction, as defined by BIP 141 */
int64_t GetTransactionWeight(const CTransaction &tx);

#endif // BITCOIN_PRIMITIVES_TRANSACTION_H
