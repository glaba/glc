**Member operators**  
_unit::field_: a field that is built-in to the game
_unit.field_: a field that is defined within a trait
_unit->field_: a field that is provided by the language

**Global variables**  
_time_: Game time

**Statements**  
_assignment_: (field) = (arithmetic / logical)
- Sets the field to the provided value as long as the statement is evaluated

_transition if_: if becomes (condition) { (body) }
- If the condition goes from being false to being true, the body is evaluated for a short period of time after that instant

_continuous if_: if (condition) { (body) }
- If the condition is true, the body is continuously evaluated as long as the condition is true

_for duration_: for (duration): { (body) } OR for (duration): (statement)
- The body is continuously evaluated starting from the moment the for duration statement is evaluated until (duration) amount of time after. The total time spent in the body for any uninterrupted stretch is exactly equal to the amount of time the for duration statement itself is evaluated plus (duration) amount of time afterwards

_for in_: for (identifier) in range of (unit) [with trait (list of traits)] { (body) }
- Evaluates the body for each unit in the specified range that has all the traits given

**Built-in traits**  
- Mechanical, Undead, Biological, Beast, Human, Flying, UniqueAndHeroic, Building, Unit
- Type\<unit id, unit id, ...\>: matches only the units with the given IDs

**Built-in fields / functions**  
- unit->age
- unit->isAlly(unit), unit->isEnemy(unit), unit->isSelf(unit)
- unit->displayModifier(name, description, image)

**Applying traits to units / buildings**  
unit / building (unit / building name) : (list of traits applying to unit / building)