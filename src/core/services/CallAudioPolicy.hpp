#pragma once

#include <QObject>

namespace oap {

class IProjectionStatusProvider;

/// Decides when HFP SCO audio should be rejected so the phone keeps call
/// audio during an Android Auto session (design §6). Mechanism (the actual
/// RejectSCO D-Bus write) lives in TelephonyClient — main.cpp connects
/// rejectScoWanted → TelephonyClient::setRejectSco.
///
/// Default policy is OFF (HFP owns call audio always, like commercial head
/// units) until live check L4 justifies flipping phone.reject_sco_during_aa.
class CallAudioPolicy : public QObject {
    Q_OBJECT
public:
    explicit CallAudioPolicy(IProjectionStatusProvider* projection,
                             bool rejectScoDuringAa, QObject* parent = nullptr);

    bool wantReject() const { return wantReject_; }

signals:
    void rejectScoWanted(bool reject);

private:
    void recompute();

    IProjectionStatusProvider* projection_;
    bool enabled_;
    bool wantReject_ = false;
};

} // namespace oap
