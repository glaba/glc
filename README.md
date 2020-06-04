**Member operators**  
_unit::field_: a field that is built-in to the game
_unit.field_: a field that is defined within a trait
_unit->field_: a field that is provided by the language

**Global variables**  
_time_: Game time

**Statements**  
_assignment_: (field) := (arithmetic / logical) OR (field) += (arithmetic)
- Sets the field to the provided value as long as the statement is evaluated OR performs a single relative modification to the field value that holds as long as the statement is evaluated (essentially the same behavior as modifiers in-game)
- Check the section on Assignments for more information

_transition if_: if becomes (condition) { (body) }
- If the condition goes from being false to being true, the body is evaluated for a short period of time after that instant

_continuous if_: if (condition) { (body) }
- If the condition is true, the body is continuously evaluated as long as the condition is true

_for duration_: for (duration): { (body) } OR for (duration): (statement)
- The body is continuously evaluated starting from the moment the for duration statement is evaluated until (duration) amount of time after. The total time spent in the body for any uninterrupted stretch is exactly equal to the amount of time the for duration statement itself is evaluated plus (duration) amount of time afterwards

_for in_: for (identifier) in range of (unit) [with trait (list of traits)] { (body) }
- Evaluates the body for each unit in the specified range that has all the traits given

**Language traits**  
- Mechanical, Undead, Biological, Beast, Human, Flying, UniqueAndHeroic, Building, Unit
- Type\<unit id, unit id, ...\>: matches only the units with the given IDs

**Language fields / functions**  
- unit->age
- unit->isAlly(unit), unit->isEnemy(unit), unit->isSelf(unit)
- unit->displayModifier(name, description, image)

**Built-in fields**  
The following are the fields can be accessed using the :: operator (built-in field member operator)
- hp : int<1, 99999999>
- mana : int<0, 99999999>
- hpRegenerationRate : float
- manaRegenerationRate : float
- armor : float
- weaponCooldown : float
- weaponDelay : float
- dmg : float
- armorPenetration : float
- dmgCap : float
- range : float
- minRange : float
- aoeRadius : float
- attackPrio : float
- imageScale : float
- repairRate : float
- repairCost : float
- projectileSpeed : float
- circleSize : float
- circleOffset : float
- drawOffsetY : float
- acceleration : float
- angularVelocity : float
- goldReward : int<0, 999999>
- controllable : bool
- hasDetection : bool
- noShow : bool
- isInvisible : bool

**Applying traits to units / buildings**  
unit / building (unit / building name) : (list of traits applying to unit / building)

**Assignments**  
_Tracking_ - using modifiers that change the rate of a field A so that the value of field A quickly approaches the value of some desired expression. This is implemented by setting the modificationRate to a small positive or negative value, if A is a bit above or a bit below the desired expression, a slightly larger value if A is more above / more below, and so on, with the modificationRates growing exponentially to minimize time spent tracking while maintaining accuracy  

_Tracking granularity_ - The smallest modificationRate used while tracking is the tracking granularity, and describes the maximum amount of precision achievable. For example, if the smallest modificationRate is 2 per tick, we can never track the desired value a precision smaller than 2. (So, an initial value of 10 attempting to target 4.5 will flip flop between 4 and 6)  

_Absolute assignment_ - an assignment that directly sets the value of the LHS. Only one can be active at a time. This is usually generated as the LHS tracking the RHS as closely as possible, unless special case optimizations are possible, which results in normal-looking behavior when the LHS and the RHS are independent  
- A := 1 - A - should flip flops between two values continuously but instead will stabilize around 0.5 (assuming sufficient tracking granularity)  
- A := A + 5 - should grow without bound and will do so  
- A := (1 + A) / A - should tend towards the golden ratio, but only by pure chance, since this expression generates an attractivee fixed point  

_Relative assignment_ - an assignment that modifies the current value of the LHS  
- A += 5 - regular +5 modifier  
- A += expr - acts as a regular relative modifier but + the value of the RHS in real time. This is implemented by having an auxilary variable tracking the RHS instead of the LHS, and both the auxilary variable and the LHS being modified at the same rate  

_Rate assignment_ - an assignment that modifies the rate of change of the LHS. It is expressed with the same syntax as a relative assignment, since it only makes a relative modification to the rate  
- A->rate += 5 - causes the base HP of the unit to increase by 5 every *second*

**Undefined behavior**  
- Accessing a custom property of a unit object declared in a for-in loop where the unit object has multiple traits with the same property name
- Setting the value of a built-in float field to a value that is out of the bounds defined in the editor (the most likely result is that it will just get clipped, but this is not guaranteed)
- Setting the value of an int field to a float
- Recursive (even indirectly recursive) variable uses in any assignments that use tracking
- Multiple conflicting assignments being active at the same time, where the table of conflicts is as follows, with A = absolute, R = relative, and T = rate assignment:  

	|   | A | R | T |
	|---|---|---|---|
	| A | X | X | X |
	| R | X | O | O |
	| T | X | O | X |
