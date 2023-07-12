
def bits_of_nodes(levels, children_per_node):
    children = 0
    for i in range(1, levels+1):
        children += children_per_node**i
    return int(children * 8 * 4 * 8)

def bits_of_bricks(levels):
    return int(8**levels * 2 * 8)

CHILDREN = 4
COLUMNS = '{: >20}' * 4

print(COLUMNS.format('Levels', 'Trees', 'Bricks', 'Ratio'))
for level in range(1, 10):
    node = bits_of_nodes(level, CHILDREN)
    brick = bits_of_bricks(level)
    ratio = brick / node
    print(COLUMNS.format(level, node, brick, ratio))
print('')