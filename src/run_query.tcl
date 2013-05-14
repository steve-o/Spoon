# I am not a comment

set from [clock scan "2013-04-16 00:00:00"]
set till [clock scan "2013-04-16 13:59:59"]
get_spoon -start=$from -end=$till -direction=0 -limit=0 -record=Trade -ric=TIBX.O --query-property=normrule=NormBarRuleForFIVE_MIN\;includehistorical=0 --not-use-time_t

# Sample output:
#
# {2125674784 19.54 29089 19.54 19.6}
# {-2048792512 19.54 29236 19.54 19.65}