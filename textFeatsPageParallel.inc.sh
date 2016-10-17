. run_parallel.inc.sh;

textFeatsPageParallel () {(
  local ARGS=();
  local OUTDIR=".";
  local SAVEXML="-";
  local THREADS=$(nproc);
  local XPATH='//_:TextRegion/_:TextLine/_:Coords[@points and @points!="0,0 0,0"]';

  local n="1";
  while [ "$n" -lt "$#" ]; do
    case "${!n}" in
      "-T" | "--threads")
        n=$((n+1));
        THREADS="${!n}";
        ;;
      "-o" | "--outdir")
        n=$((n+1));
        OUTDIR="${!n}";
        ;;
      "--xpath")
        n=$((n+1));
        XPATH="${!n}";
        ;;
      *)
        ARGS+=( "${!n}" );
        ;;
    esac
    [ "${!n:0:10}" = "--savexml" ] && SAVEXML="${!n:10}";
    n=$((n+1));
  done

  local XML="${!#}";
  local NUMLINES=$(xmlstarlet sel -t -v "count($XPATH)" "$XML");
  [ "$NUMLINES" = "" ] && return 1;

  local LST=$( awk -v NLIST=$NUMLINES -v NTHREADS=$THREADS '
    function ceil(v) { return int(v) + (v == int(v) ? 0 : 1); }
    BEGIN {
      fact = ceil(NLIST/NTHREADS);
      while( fact < 2 && NTHREADS > 1 ) {
        NTHREADS--;
        fact = ceil(NLIST/NTHREADS);
      }
      if ( NTHREADS == 1 )
        printf( "1-%d\n", NLIST );
      else {
        n = 0;
        while( n < NLIST ) {
          nxt = n+fact;
          printf( "%s%d-%d", n==0?"":",", n+1, ( nxt < NLIST ? nxt : NLIST ) );
          n = nxt;
        }
        printf("\n");
      }
    }' );

  extract_feats () {
    local RNG=( ${1/-/ } );
    local LARGS=( "${ARGS[@]}" --outdir "$OUTDIR" --xpath "($XPATH)[position() >= ${RNG[0]} and not(position() > ${RNG[1]})]" );
    [ "$SAVEXML" != "-" ] &&
      local TMP=$(mktemp -d --tmpdir="$OUTDIR" extract_feats_XXXXX) &&
      LARGS+=( --savexml="$TMP" );
    { textFeats "${LARGS[@]}" "$XML";
      [ "$?" != 0 ] && return 1;
    } | sed "s|/extract_feats_...../|/|";
    #if [ -f $(echo "$XML" | sed 's|.*/||; s|^|'"$TMP"/'|;') ]; then
    #fi
    #[ "$SAVEXML" != "-" ] &&
      rmdir -r "$TMP";
    return 0;
  }

  local TMPDIR="$OUTDIR";
  run_parallel -T "$THREADS" -l "$LST" -p no \
    extract_feats '{*}';
)}
