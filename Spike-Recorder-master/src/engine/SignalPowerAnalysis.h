#ifndef BACKYARDBRAINS_SIGNALPOWERANALYSIS_H
#define BACKYARDBRAINS_SIGNALPOWERANALYSIS_H

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdint.h>

namespace BackyardBrains {
namespace SignalPower {

// AC amplitude of the selected, already-filtered channel. Keep ADC units here;
// any voltage calibration belongs to the presentation layer.
struct Reading {
    double mean, power, rms, mav, peak, dbfs;
    bool valid, rangeLimit;
    Reading() : mean(0), power(0), rms(0), mav(0), peak(0), dbfs(-120), valid(false), rangeLimit(false) {}
};

inline Reading measure(const int16_t *data, size_t count) {
    Reading r;
    if(!data || count < 2) return r;
    for(size_t i=0;i<count;++i) r.mean += data[i];
    r.mean /= count;
    for(size_t i=0;i<count;++i) {
        const double value = static_cast<double>(data[i]) - r.mean;
        r.power += value*value;
        r.mav += std::fabs(value);
        r.peak = std::max(r.peak,std::fabs(value));
        r.rangeLimit = r.rangeLimit || data[i] >= 32760 || data[i] <= -32760;
    }
    r.power /= count;
    r.mav /= count;
    r.rms = std::sqrt(r.power);
    r.dbfs = r.rms > 0 ? std::max(-120.0,20*std::log10(r.rms/32768.0)) : -120.0;
    r.valid = true;
    return r;
}

// Reference-relative amplitude, not force, watts, or %MVC. Rest noise is
// removed in quadrature under the assumption of additive independent noise.
class RelativeActivity {
public:
    enum Stage { Idle, Rest, Reference };
    enum Result { None, RestSaved, ReferenceSaved, NeedRest, TooWeak, RangeLimit, Interrupted };
    RelativeActivity() { reset(); }
    void reset() {
        _stage=Idle; _result=None; _restReady=_referenceReady=false;
        _restPower=_referencePower=_elapsed=_sum=_weight=0;
    }
    bool start(Stage stage) {
        if(stage==Idle) return false;
        if(stage==Reference && !_restReady) { _result=NeedRest; return false; }
        _stage=stage; _result=None; _elapsed=_sum=_weight=0;
        _referenceReady=false;
        if(stage==Rest) _restReady=false;
        return true;
    }
    void cancel() {
        if(_stage!=Idle) { _stage=Idle; _result=Interrupted; }
    }
    void ingest(const Reading &r, double newSeconds) {
        if(_stage==Idle || !r.valid || !(newSeconds>0)) return;
        if(!std::isfinite(newSeconds) || newSeconds>.5) { cancel(); return; }
        if(r.rangeLimit) { _stage=Idle; _result=RangeLimit; return; }
        const double duration = _stage==Rest ? 2.0 : 3.0;
        const double previous=_elapsed;
        _elapsed=std::min(duration,_elapsed+newSeconds);
        // Do not include a rolling window which still contains the previous
        // task. The acquisition timer advances only with genuinely new samples.
        const double weight=std::max(0.0,_elapsed-std::max(previous,.25));
        _sum += r.power*weight; _weight += weight;
        if(_elapsed+1e-9 < duration) return;
        const Stage completed=_stage;
        _stage=Idle;
        const double power=_weight>0 ? _sum/_weight : 0;
        if(completed==Rest) {
            _restPower=power; _restReady=true; _result=RestSaved;
        } else {
            const double restRms=std::sqrt(_restPower);
            const double minimum=std::max(restRms*1.5,restRms+5.0);
            if(std::sqrt(power)<minimum) { _result=TooWeak; return; }
            _referencePower=power; _referenceReady=true; _result=ReferenceSaved;
        }
    }
    bool ready() const { return _restReady && _referenceReady; }
    bool restReady() const { return _restReady; }
    Stage stage() const { return _stage; }
    Result result() const { return _result; }
    double progress() const { return _stage==Idle ? 0 : _elapsed/(_stage==Rest?2.0:3.0); }
    double remaining() const { return _stage==Idle ? 0 : std::max(0.0,(_stage==Rest?2.0:3.0)-_elapsed); }
    double restRms() const { return std::sqrt(_restPower); }
    double referenceRms() const { return std::sqrt(_referencePower); }
    double percent(const Reading &r) const {
        if(!ready() || !r.valid) return 0;
        const double denominator=_referencePower-_restPower;
        return denominator>0 ? 100*std::sqrt(std::max(0.0,r.power-_restPower)/denominator) : 0;
    }
private:
    Stage _stage;
    Result _result;
    bool _restReady,_referenceReady;
    double _restPower,_referencePower,_elapsed,_sum,_weight;
};

} // namespace SignalPower
} // namespace BackyardBrains
#endif
