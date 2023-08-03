
def bits_of_nodes(levels, children_per_node):
    children = 0
    for i in range(1, levels+1):
        children += children_per_node**i
    return int(children * 8 * 4 * 8)

def bits_of_bricks(levels):
    return int(8**levels * 12)

CHILDREN = 4
COLUMNS = '{: >16}' * 5

print(COLUMNS.format('Levels', 'Trees', 'Bricks', 'Ratio', 'Ratio (brick-1)'))
for level in range(1, 7):
    node = bits_of_nodes(level, CHILDREN)
    brick = bits_of_bricks(level)
    ratio = round(brick / node, 3)
    nodeprevbrick = 32 * 8 + CHILDREN * bits_of_bricks(level-1)
    prevratio = round(brick / nodeprevbrick, 3)
    print(COLUMNS.format(level, node, brick, ratio, prevratio))
print('')