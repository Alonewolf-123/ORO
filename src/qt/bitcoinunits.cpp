// Copyright (c) 2011-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2017 The PIVX developers
// Copyright (c) 2018-2019 The ORO developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "bitcoinunits.h"
#include "chainparams.h"
#include "primitives/transaction.h"

#include <QSettings>
#include <QStringList>

BitcoinUnits::BitcoinUnits(QObject* parent) : QAbstractListModel(parent),
                                              unitlist(availableUnits())
{
}

QList<BitcoinUnits::Unit> BitcoinUnits::availableUnits()
{
    QList<BitcoinUnits::Unit> unitlist;
    unitlist.append(ORO);
    unitlist.append(mORO);
    return unitlist;
}

bool BitcoinUnits::valid(int unit)
{
    switch (unit) {
    case ORO:
    case mORO:
        return true;
    default:
        return false;
    }
}

QString BitcoinUnits::id(int unit)
{
    switch (unit) {
    case ORO:
        return QString("oro");
    case mORO:
        return QString("moro");
    default:
        return QString("???");
    }
}

QString BitcoinUnits::name(int unit)
{
    if (Params().NetworkID() == CBaseChainParams::MAIN) {
        switch (unit) {
        case ORO:
            return QString("ORO");
        case mORO:
            return QString("mORO");
        default:
            return QString("???");
        }
    } else {
        switch (unit) {
        case ORO:
            return QString("tORO");
        case mORO:
            return QString("mtORO");
        default:
            return QString("???");
        }
    }
}

QString BitcoinUnits::description(int unit)
{
    if (Params().NetworkID() == CBaseChainParams::MAIN) {
        switch (unit) {
        case ORO:
            return QString("ORO");
        case mORO:
            return QString("Milli-ORO (1 / 1" THIN_SP_UTF8 "000)");
        default:
            return QString("???");
        }
    } else {
        switch (unit) {
        case ORO:
            return QString("TestOROs");
        case mORO:
            return QString("Milli-TestORO (1 / 1" THIN_SP_UTF8 "000)");
        default:
            return QString("???");
        }
    }
}

qint64 BitcoinUnits::factor(int unit)
{
    switch (unit) {
    case ORO:
        return 1000;
    case mORO:
        return 1;
    default:
        return 1000;
    }
}

int BitcoinUnits::decimals(int unit)
{
    switch (unit) {
    case ORO:
        return 3;
    case mORO:
        return 0;
    default:
        return 0;
    }
}

QString BitcoinUnits::format(int unit, const CAmount& nIn, bool fPlus, SeparatorStyle separators)
{
    // Note: not using straight sprintf here because we do NOT want
    // localized number formatting.
    if (!valid(unit))
        return QString(); // Refuse to format invalid unit
    qint64 n = (qint64)nIn;
    qint64 coin = factor(unit);
    int num_decimals = decimals(unit);
    qint64 n_abs = (n > 0 ? n : -n);
    qint64 quotient = n_abs / coin;
    qint64 remainder = n_abs % coin;
    QString quotient_str = QString::number(quotient);
    QString remainder_str = QString::number(remainder).rightJustified(num_decimals, '0');

    // Use SI-style thin space separators as these are locale independent and can't be
    // confused with the decimal marker.
    QChar thin_sp(THIN_SP_CP);
    QChar comma(COMMA);
    int q_size = quotient_str.size();
    if (separators == separatorAlways || (separators == separatorStandard && q_size >= 4))
        for (int i = 3; i < q_size; i += 3)
            quotient_str.insert(q_size - i, thin_sp);
    else if (separators == separatorComma && q_size >= 4)
            for (int i = 3; i < q_size; i += 3)
                quotient_str.insert(q_size - i, comma);

    if (n < 0)
        quotient_str.insert(0, '-');
    else if (fPlus && n > 0)
        quotient_str.insert(0, '+');

    if (num_decimals <= 0)
        return quotient_str;

    return quotient_str + QString(".") + remainder_str;
}


// TODO: Review all remaining calls to BitcoinUnits::formatWithUnit to
// TODO: determine whether the output is used in a plain text context
// TODO: or an HTML context (and replace with
// TODO: BtcoinUnits::formatHtmlWithUnit in the latter case). Hopefully
// TODO: there aren't instances where the result could be used in
// TODO: either context.

// NOTE: Using formatWithUnit in an HTML context risks wrapping
// quantities at the thousands separator. More subtly, it also results
// in a standard space rather than a thin space, due to a bug in Qt's
// XML whitespace canonicalisation
//
// Please take care to use formatHtmlWithUnit instead, when
// appropriate.

QString BitcoinUnits::formatWithUnit(int unit, const CAmount& amount, bool plussign, SeparatorStyle separators)
{
    return format(unit, amount, plussign, separators) + QString(" ") + name(unit);
}

QString BitcoinUnits::formatHtmlWithUnit(int unit, const CAmount& amount, bool plussign, SeparatorStyle separators)
{
    QString str(formatWithUnit(unit, amount, plussign, separators));
    str.replace(QChar(THIN_SP_CP), QString(THIN_SP_HTML));
    return QString("<span style='white-space: nowrap;'>%1</span>").arg(str);
}

QString BitcoinUnits::floorWithUnit(int unit, const CAmount& amount, bool plussign, SeparatorStyle separators)
{
    QSettings settings;
    int digits = settings.value("digits").toInt();

    QString result = format(unit, amount, plussign, separators);
    if (decimals(unit) > digits) result.chop(decimals(unit) - digits);

    return result + QString(" ") + name(unit);
}

QString BitcoinUnits::floorHtmlWithUnit(int unit, const CAmount& amount, bool plussign, SeparatorStyle separators)
{
    QString str(floorWithUnit(unit, amount, plussign, separators));
    str.replace(QChar(THIN_SP_CP), QString(THIN_SP_HTML));
    return QString("<span style='white-space: nowrap;'>%1</span>").arg(str);
}

bool BitcoinUnits::parse(int unit, const QString& value, CAmount* val_out)
{
    if (!valid(unit) || value.isEmpty())
        return false; // Refuse to parse invalid unit or empty string
    int num_decimals = decimals(unit);

    // Ignore spaces and thin spaces when parsing
    QStringList parts = removeSpaces(value).split(".");

    if (parts.size() > 2) {
        return false; // More than one dot
    }
    QString whole = parts[0];
    QString decimals;

    if (parts.size() > 1) {
        decimals = parts[1];
    }
    if (decimals.size() > num_decimals) {
        return false; // Exceeds max precision
    }
    bool ok = false;
    QString str = whole + decimals.leftJustified(num_decimals, '0');

    if (str.size() > 18) {
        return false; // Longer numbers will exceed 63 bits
    }
    CAmount retvalue(str.toLongLong(&ok));
    if (val_out) {
        *val_out = retvalue;
    }
    return ok;
}

QString BitcoinUnits::getAmountColumnTitle(int unit)
{
    QString amountTitle = QObject::tr("Amount");
    if (BitcoinUnits::valid(unit)) {
        amountTitle += " (" + BitcoinUnits::name(unit) + ")";
    }
    return amountTitle;
}

int BitcoinUnits::rowCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return unitlist.size();
}

QVariant BitcoinUnits::data(const QModelIndex& index, int role) const
{
    int row = index.row();
    if (row >= 0 && row < unitlist.size()) {
        Unit unit = unitlist.at(row);
        switch (role) {
        case Qt::EditRole:
        case Qt::DisplayRole:
            return QVariant(name(unit));
        case Qt::ToolTipRole:
            return QVariant(description(unit));
        case UnitRole:
            return QVariant(static_cast<int>(unit));
        }
    }
    return QVariant();
}

CAmount BitcoinUnits::maxMoney()
{
    return Params().MaxMoneyOut();
}

USDUnits::USDUnits(QObject *parent):
        QAbstractListModel(parent),
        unitlist(availableUnits())
{
}

QList<USDUnits::Unit> USDUnits::availableUnits()
{
    QList<USDUnits::Unit> unitlist;
    unitlist.append(USD);
    unitlist.append(UScent);
    return unitlist;
}

bool USDUnits::valid(int unit)
{
    switch(unit)
    {
    case USD:
    case UScent:
        return true;
    default:
        return false;
    }
}

QString USDUnits::id(int unit)
{
    switch(unit)
    {
    case USD: return QString("usd");
    case UScent: return QString("us cent");
    default: return QString("???");
    }
}

QString USDUnits::name(int unit)
{
    switch(unit)
    {
    case USD: return QString("USD");
    case UScent: return QString("US cent");
    default: return QString("???");
    }
}

QString USDUnits::description(int unit)
{
    switch(unit)
    {
    case USD: return QString("US Dollars");
    case UScent: return QString("US Dollar Cents (1 / 100");
    default: return QString("???");
    }
}

qint64 USDUnits::factor(int unit)
{
    switch(unit)
    {
    case USD:    return 1000;
    case UScent: return 10;
    default:   return 1000;
    }
}

int USDUnits::decimals(int unit)
{
    switch(unit)
    {
    case USD: return 3;
    case UScent: return 1;
    default: return 0;
    }
}

QString USDUnits::format(int unit, const CAmount& nIn, bool fPlus, SeparatorStyle separators, bool omitRemainderIfZero, bool forceOmitRemainder)
{
    // Note: not using straight sprintf here because we do NOT want
    // localized number formatting.
    if(!valid(unit))
        return QString(); // Refuse to format invalid unit
    qint64 n = (qint64)nIn;
    qint64 coin = factor(unit);
    int num_decimals = decimals(unit);
    qint64 n_abs = (n > 0 ? n : -n);
    qint64 quotient = n_abs / coin;
    qint64 remainder = n_abs % coin;
    QString quotient_str = QString::number(quotient);
    QString remainder_str = QString::number(remainder).rightJustified(num_decimals, '0');

    // Use SI-style thin space separators as these are locale independent and can't be
    // confused with the decimal marker.
    QChar thin_sp(THIN_SP_CP);
    QChar comma(COMMA);

    int q_size = quotient_str.size();
    if (separators == separatorAlways || (separators == separatorStandard && q_size >= 4))
        for (int i = 3; i < q_size; i += 3)
            quotient_str.insert(q_size - i, thin_sp);
    else if (separators == separatorComma && q_size >= 4)
        for (int i = 3; i < q_size; i += 3)
            quotient_str.insert(q_size - i, comma);


    if (n < 0)
        quotient_str.insert(0, '-');
    else if (fPlus && n > 0)
        quotient_str.insert(0, '+');
    return quotient_str + ((forceOmitRemainder || (omitRemainderIfZero && remainder == 0)) ? QString("") : QString(".") + remainder_str);
}


// TODO: Review all remaining calls to USDUnits::formatWithUnit to
// TODO: determine whether the output is used in a plain text context
// TODO: or an HTML context (and replace with
// TODO: BtcoinUnits::formatHtmlWithUnit in the latter case). Hopefully
// TODO: there aren't instances where the result could be used in
// TODO: either context.

// NOTE: Using formatWithUnit in an HTML context risks wrapping
// quantities at the thousands separator. More subtly, it also results
// in a standard space rather than a thin space, due to a bug in Qt's
// XML whitespace canonicalisation
//
// Please take care to use formatHtmlWithUnit instead, when
// appropriate.

QString USDUnits::formatWithUnit(int unit, const CAmount& amount, bool plussign, SeparatorStyle separators, bool omitRemainderIfZero, bool forceOmitRemainder)
{
    return format(unit, amount, plussign, separators, omitRemainderIfZero, forceOmitRemainder) + QString(" ") + name(unit);
}

QString USDUnits::formatHtmlWithUnit(int unit, const CAmount& amount, bool plussign, SeparatorStyle separators)
{
    QString str(formatWithUnit(unit, amount, plussign, separators));
    str.replace(QChar(THIN_SP_CP), QString(THIN_SP_HTML));
    return QString("<span style='white-space: nowrap;'>%1</span>").arg(str);
}


bool USDUnits::parse(int unit, const QString &value, CAmount *val_out)
{
    if(!valid(unit) || value.isEmpty())
        return false; // Refuse to parse invalid unit or empty string
    int num_decimals = decimals(unit);

    // Ignore spaces and thin spaces when parsing
    QStringList parts = removeSpaces(value).split(".");

    if(parts.size() > 2)
    {
        return false; // More than one dot
    }
    QString whole = parts[0];
    QString decimals;

    if(parts.size() > 1)
    {
        decimals = parts[1];
    }
    if(decimals.size() > num_decimals)
    {
        return false; // Exceeds max precision
    }
    bool ok = false;
    QString str = whole + decimals.leftJustified(num_decimals, '0');

    if(str.size() > 18)
    {
        return false; // Longer numbers will exceed 63 bits
    }
    CAmount retvalue(str.toLongLong(&ok));
    if(val_out)
    {
        *val_out = retvalue;
    }
    return ok;
}

QString USDUnits::getAmountColumnTitle(int unit)
{
    QString amountTitle = QObject::tr("Amount");
    if (USDUnits::valid(unit))
    {
        amountTitle += " ("+USDUnits::name(unit) + ")";
    }
    return amountTitle;
}

int USDUnits::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return unitlist.size();
}

QVariant USDUnits::data(const QModelIndex &index, int role) const
{
    int row = index.row();
    if(row >= 0 && row < unitlist.size())
    {
        Unit unit = unitlist.at(row);
        switch(role)
        {
        case Qt::EditRole:
        case Qt::DisplayRole:
            return QVariant(name(unit));
        case Qt::ToolTipRole:
            return QVariant(description(unit));
        case UnitRole:
            return QVariant(static_cast<int>(unit));
        }
    }
    return QVariant();
}

CAmount USDUnits::maxMoney()
{
    return MAX_MONEY;
}
