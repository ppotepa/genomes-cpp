import polygonClipping from 'polygon-clipping';
import earcut from 'earcut';
import * as kiwi from '@lume/kiwi';
import * as navcat from 'navcat';
import * as navcatBlocks from 'navcat/blocks';
import * as straightSkeleton from 'straight-skeleton';

globalThis.RTSBuildingVendors = Object.freeze({polygonClipping, earcut, kiwi, navcat, navcatBlocks, straightSkeleton});
