from common_def.dbc_seed import  *
net_type="test"  #main, test
g_dns=[]
g_seeds_v4=[]
g_seeds_v6=[]
if net_type == "test":
    g_dns = test_dns
    g_seeds_v4 = test_seed_v4
    g_seeds_v6 = test_seed_v6
if net_type == "main":
    g_dns = main_dns
    g_seeds_v4 = main_seed_v4
    g_seeds_v6 = main_seed_v6
