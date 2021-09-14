SELECT P2.name
  FROM Pokemon P1
     , Evolution E1
     , Evolution E2
     , Pokemon P2
 WHERE P1.id = E1.before_id
   AND E1.after_id = E2.before_id
   AND E2.after_id = P2.id
   AND P1.name = 'Charmander';
