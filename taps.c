#include "defines.h"

/// <summary>
/// LPF, 384000 Hz, 100 kHz to 120 kHz at 50 dB attenuation
/// </summary>
const float taps_demod[NTAPS_DEMOD] = {
	0.00023626783513464034,0.0013071773573756218,-0.00054017617367208,-0.002048673341050744,0.001322773634456098,0.0034895429853349924,-0.002981264377012849,-0.0055846828036010265,0.005997146479785442,0.008198215626180172,-0.010986482724547386,-0.01111582014709711,0.018866021186113358,0.014068453572690487,-0.03136329725384712,-0.01676447130739689,0.05281371250748634,0.01892584189772606,-0.09919781237840652,-0.02032349817454815,0.3155951201915741,0.5201718211174011,0.3155951201915741,-0.02032349817454815,-0.09919781237840652,0.01892584189772606,0.05281371250748634,-0.01676447130739689,-0.03136329725384712,0.014068453572690487,0.018866021186113358,-0.01111582014709711,-0.010986482724547386,0.008198215626180172,0.005997146479785442,-0.0055846828036010265,-0.002981264377012849,0.0034895429853349924,0.001322773634456098,-0.002048673341050744,-0.00054017617367208,0.0013071773573756218,0.00023626783513464034
};

const float taps_composite[NTAPS_COMPOSITE] = {
	-0.0001460164930904284,3.4673935277817396e-19,0.00014885782729834318,0.00016768413479439914,3.5992539778817445e-05,-0.00013294679229147732,-0.00018839011318050325,-7.527358684455976e-05,0.00011215917038498446,0.00020763698557857424,0.00011887954315170646,-8.453996997559443e-05,-0.0002240664034616202,-0.0001673128572292626,4.787607031175867e-05,0.00023543494171462953,0.0002202665200456977,-1.3503157915184344e-18,-0.00023869001597631723,-0.00027639855397865176,-6.086247230996378e-05,0.0002301486674696207,0.0003331821062602103,0.00013572769239544868,-0.00020577420946210623,-0.0003868529456667602,-0.00022450246615335345,0.00016153804608620703,0.0004324700857978314,0.000325677334330976,-9.384565055370331e-05,-0.00046409748028963804,-0.0004360951134003699,4.291889473957938e-18,0.0004751081869471818,0.0005508231115527451,0.00012133023119531572,-0.0004586000577546656,-0.0006631514406763017,-0.00026967297890223563,0.00040790732600726187,0.0007647337042726576,0.0004423823265824467,-0.0003171804128214717,-0.0008458767551928759,-0.0006343700224533677,0.0001820019824663177,0.0008959774277172983,0.0008379709324799478,-1.1285930682123464e-17,-0.0009040948934853077,-0.0010429663816466928,-0.0002285828668391332,0.0008596344850957394,0.0012367834569886327,0.0005004065460525453,-0.000753114465624094,-0.001404878101311624,-0.000808676821179688,0.0005769747076556087,0.0015312953619286418,0.0011429514270275831,-0.0003263843827880919,-0.0015993969282135367,-0.0014891359023749828,-9.097145227244608e-19,0.0015927271451801062,0.0018296892521902919,0.00039937286055646837,-0.0014959810068830848,-0.0021440479904413223,-0.0008642603643238544,0.0012960362946614623,0.0024092623498290777,0.0013821835163980722,-0.0009829915361478925,-0.0026008314453065395,-0.0019355331314727664,0.0005511654308065772,0.002693705027922988,0.002501679817214608,-7.60402735291928e-18,-0.0026634112000465393,-0.0030533215031027794,-0.0006651807925663888,0.002487265272065997,0.0035590650513768196,0.0014325958909466863,-0.002145595382899046,-0.003984210547059774,-0.002283648820593953,0.0016229272587224841,0.004291704390197992,0.0031927989330142736,-0.0009090638486668468,-0.004443190060555935,-0.004127655178308487,2.762247563762423e-17,0.0044000884518027306,0.005049269180744886,0.0011013849871233106,-0.004124590195715427,-0.005912553519010544,-0.0023849017452448606,0.003580436808988452,0.006666727364063263,0.003832928603515029,-0.0027333085890859365,-0.0072556170634925365,-0.005420610774308443,0.0015505712945014238,0.007617529947310686,0.007116393186151981,-1.0887664723875108e-17,-0.007684262935072184,-0.008882864378392696,-0.0019531541038304567,0.007378438487648964,0.010677899233996868,0.004351911135017872,-0.006607711780816317,-0.012456046417355537,-0.007258438039571047,0.005252804607152939,0.014170111157000065,0.010775077156722546,-0.003142657922580838,-0.01577286422252655,-0.015087642706930637,1.1847993290908346e-17,0.017218807712197304,0.02056381106376648,0.004690094385296106,-0.018465908244252205,-0.028011150658130646,-0.012049729004502296,0.01947723887860775,0.03951297700405121,0.02512899972498417,-0.020222414284944534,-0.062267985194921494,-0.05617048218846321,0.02067882940173149,0.14697088301181793,0.26462632417678833,0.3124880790710449,0.26462632417678833,0.14697088301181793,0.02067882940173149,-0.05617048218846321,-0.062267985194921494,-0.020222414284944534,0.02512899972498417,0.03951297700405121,0.01947723887860775,-0.012049729004502296,-0.028011150658130646,-0.018465908244252205,0.004690094385296106,0.02056381106376648,0.017218807712197304,1.1847993290908346e-17,-0.015087642706930637,-0.01577286422252655,-0.003142657922580838,0.010775077156722546,0.014170111157000065,0.005252804607152939,-0.007258438039571047,-0.012456046417355537,-0.006607711780816317,0.004351911135017872,0.010677899233996868,0.007378438487648964,-0.0019531541038304567,-0.008882864378392696,-0.007684262935072184,-1.0887664723875108e-17,0.007116393186151981,0.007617529947310686,0.0015505712945014238,-0.005420610774308443,-0.0072556170634925365,-0.0027333085890859365,0.003832928603515029,0.006666727364063263,0.003580436808988452,-0.0023849017452448606,-0.005912553519010544,-0.004124590195715427,0.0011013849871233106,0.005049269180744886,0.0044000884518027306,2.762247563762423e-17,-0.004127655178308487,-0.004443190060555935,-0.0009090638486668468,0.0031927989330142736,0.004291704390197992,0.0016229272587224841,-0.002283648820593953,-0.003984210547059774,-0.002145595382899046,0.0014325958909466863,0.0035590650513768196,0.002487265272065997,-0.0006651807925663888,-0.0030533215031027794,-0.0026634112000465393,-7.60402735291928e-18,0.002501679817214608,0.002693705027922988,0.0005511654308065772,-0.0019355331314727664,-0.0026008314453065395,-0.0009829915361478925,0.0013821835163980722,0.0024092623498290777,0.0012960362946614623,-0.0008642603643238544,-0.0021440479904413223,-0.0014959810068830848,0.00039937286055646837,0.0018296892521902919,0.0015927271451801062,-9.097145227244608e-19,-0.0014891359023749828,-0.0015993969282135367,-0.0003263843827880919,0.0011429514270275831,0.0015312953619286418,0.0005769747076556087,-0.000808676821179688,-0.001404878101311624,-0.000753114465624094,0.0005004065460525453,0.0012367834569886327,0.0008596344850957394,-0.0002285828668391332,-0.0010429663816466928,-0.0009040948934853077,-1.1285930682123464e-17,0.0008379709324799478,0.0008959774277172983,0.0001820019824663177,-0.0006343700224533677,-0.0008458767551928759,-0.0003171804128214717,0.0004423823265824467,0.0007647337042726576,0.00040790732600726187,-0.00026967297890223563,-0.0006631514406763017,-0.0004586000577546656,0.00012133023119531572,0.0005508231115527451,0.0004751081869471818,4.291889473957938e-18,-0.0004360951134003699,-0.00046409748028963804,-9.384565055370331e-05,0.000325677334330976,0.0004324700857978314,0.00016153804608620703,-0.00022450246615335345,-0.0003868529456667602,-0.00020577420946210623,0.00013572769239544868,0.0003331821062602103,0.0002301486674696207,-6.086247230996378e-05,-0.00027639855397865176,-0.00023869001597631723,-1.3503157915184344e-18,0.0002202665200456977,0.00023543494171462953,4.787607031175867e-05,-0.0001673128572292626,-0.0002240664034616202,-8.453996997559443e-05,0.00011887954315170646,0.00020763698557857424,0.00011215917038498446,-7.527358684455976e-05,-0.00018839011318050325,-0.00013294679229147732,3.5992539778817445e-05,0.00016768413479439914,0.00014885782729834318,3.4673935277817396e-19,-0.0001460164930904284
};