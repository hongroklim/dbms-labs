SELECT T.name
  FROM Trainer T
 WHERE T.hometown = 'Sangnok City'
   AND EXISTS
       (SELECT NULL
          FROM CatchedPokemon CP
             , Pokemon P
         WHERE CP.pid = P.id
           AND CP.owner_id = T.id
           AND P.name LIKE 'P%'
       )
 ORDER BY T.name;
