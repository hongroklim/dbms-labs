SELECT AVG(CP.level)
  FROM CatchedPokemon CP
     , Pokemon P
 WHERE CP.pid = P.id
   AND P.type = 'Water';
