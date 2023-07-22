valid = ['A', 'B', 'D', '', 'E']
test = ['A', 'E', '']

def verif_all(user_configs, tpo_configs):
    return all((cfg in tpo_configs) for cfg in user_configs)
def verif_any(user_configs, tpo_configs):
    return any((cfg in tpo_configs) for cfg in user_configs)

print(verif_all(test, valid))
print(verif_any(test, valid))
