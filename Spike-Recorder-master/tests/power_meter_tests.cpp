#include "engine/SignalPowerAnalysis.h"
#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace BackyardBrains::SignalPower;
static int passed=0,failed=0;
static void check(bool result,const char *message) { if(!result) throw std::runtime_error(message); }
static bool near(double a,double b,double tolerance=1e-6) { return std::fabs(a-b)<tolerance; }
static void test(const char *name,const std::function<void()> &f) {
    try { f(); ++passed; std::cout<<"PASS "<<name<<"\n"; }
    catch(const std::exception &e) { ++failed; std::cout<<"FAIL "<<name<<": "<<e.what()<<"\n"; }
}
static Reading square(int amplitude,int dc=0) {
    int16_t data[]={static_cast<int16_t>(dc+amplitude),static_cast<int16_t>(dc-amplitude),static_cast<int16_t>(dc+amplitude),static_cast<int16_t>(dc-amplitude)};
    return measure(data,4);
}
static void feed(RelativeActivity &m,const Reading &r,int windows) { for(int i=0;i<windows;++i) m.ingest(r,.05); }
static RelativeActivity calibrated() {
    RelativeActivity m; m.start(RelativeActivity::Rest); feed(m,square(10),40);
    m.start(RelativeActivity::Reference); feed(m,square(100),60); return m;
}
int main() {
    test("AC RMS and mean-square power",[] {
        auto r=square(100); check(r.valid && near(r.rms,100) && near(r.power,10000) && near(r.mav,100) && near(r.peak,100),"Analytic square-wave features disagree");
    });
    test("DC offset removal",[] {
        auto a=square(100),b=square(100,20000); check(near(a.power,b.power) && near(b.mean,20000),"DC offset changes AC power");
    });
    test("Constant input has zero AC power",[] { int16_t d[]={32000,32000,32000}; check(near(measure(d,3).power,0),"Constant offset reported as activity"); });
    test("Sine RMS and power",[] {
        std::vector<int16_t> d(1000); for(int i=0;i<1000;++i) d[i]=10000*std::sin(2*3.141592653589793*10*i/1000);
        auto r=measure(d.data(),d.size()); check(std::fabs(r.rms-10000/std::sqrt(2.0))<1,"Sine RMS differs from A/sqrt(2)");
    });
    test("Doubling amplitude quadruples signal power",[] { check(near(square(200).power/square(100).power,4),"Power is not quadratic"); });
    test("Signed full-range samples avoid integer overflow",[] {
        int16_t d[]={-32768,32767}; auto r=measure(d,2); check(r.valid && r.rangeLimit && near(r.rms,32767.5) && std::isfinite(r.power),"Extreme ADC samples overflow");
    });
    test("Empty or single-sample readings are invalid",[] {
        int16_t d=0; check(!measure(nullptr,0).valid && !measure(&d,1).valid,"Insufficient samples treated as valid");
    });
    test("Rest required before reference",[] { RelativeActivity m; check(!m.start(RelativeActivity::Reference) && m.result()==RelativeActivity::NeedRest && !m.ready(),"Reference accepted without resting level"); });
    test("Two-second rest acquisition",[] {
        RelativeActivity m; m.start(RelativeActivity::Rest); feed(m,square(10),39); check(!m.restReady(),"Rest completed too early");
        feed(m,square(10),1); check(m.restReady() && near(m.restRms(),10) && !m.ready(),"Rest calibration not stored");
    });
    test("Three-second reference acquisition",[] {
        RelativeActivity m; m.start(RelativeActivity::Rest); feed(m,square(10),40); m.start(RelativeActivity::Reference);
        feed(m,square(100),59); check(!m.ready(),"Reference completed too early"); feed(m,square(100),1); check(m.ready(),"Reference did not complete");
    });
    test("Noise floor maps to zero and reference to 100 percent",[] {
        auto m=calibrated(); check(near(m.percent(square(10)),0) && near(m.percent(square(100)),100),"Reference endpoints are wrong");
    });
    test("Quadrature baseline correction",[] {
        auto m=calibrated(); auto r=square(50); check(near(m.percent(r),100*std::sqrt(2400.0/9900.0)),"Relative amplitude formula is incorrect");
    });
    test("Over-reference activity remains above 100 percent",[] {
        auto m=calibrated(); check(m.percent(square(200))>200,"Displayed activity was capped or auto-rescaled");
    });
    test("Weak reference is rejected",[] {
        auto m=calibrated(); m.start(RelativeActivity::Reference); feed(m,square(12),60); check(!m.ready() && m.result()==RelativeActivity::TooWeak,"Noise-like reference was accepted");
    });
    test("Range-limited calibration is rejected",[] {
        RelativeActivity m; m.start(RelativeActivity::Rest); m.ingest(square(32767),.05);
        check(!m.restReady() && m.stage()==RelativeActivity::Idle && m.result()==RelativeActivity::RangeLimit,"Saturated reference was accepted");
    });
    test("Repeated positions do not advance calibration",[] {
        RelativeActivity m; m.start(RelativeActivity::Rest); for(int i=0;i<1000;++i) m.ingest(square(10),0);
        check(m.progress()==0 && !m.restReady(),"No-data interval advances timer");
    });
    test("Data gap cancels an incomplete calibration",[] {
        RelativeActivity m; m.start(RelativeActivity::Rest); m.ingest(square(10),1);
        check(m.stage()==RelativeActivity::Idle && m.result()==RelativeActivity::Interrupted && !m.restReady(),"Missing data accepted as continuous acquisition");
    });
    test("Pause cancellation does not store a partial reference",[] {
        auto m=calibrated(); m.start(RelativeActivity::Reference); feed(m,square(200),20); m.cancel(); check(!m.ready() && m.restReady(),"Incomplete reference remained valid");
    });
    test("A new rest measurement clears the previous reference",[] {
        auto m=calibrated(); m.start(RelativeActivity::Rest); check(!m.ready() && !m.restReady(),"Old reference survived recalibration");
    });
    test("Reset clears calibration",[] {
        auto m=calibrated(); m.reset(); check(!m.ready() && !m.restReady() && m.stage()==RelativeActivity::Idle,"Old session calibration survived reset");
    });
    test("Transition window is excluded",[] {
        RelativeActivity m; m.start(RelativeActivity::Rest); feed(m,square(1000),5); feed(m,square(10),35);
        check(m.restReady() && near(m.restRms(),10),"Old contraction contaminated resting reference");
    });
    std::cout<<"RESULT "<<passed<<" passed, "<<failed<<" failed\n"; return failed?1:0;
}
