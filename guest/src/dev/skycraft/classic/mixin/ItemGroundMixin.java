package dev.skycraft.classic.mixin;
import dev.skycraft.link.SkyLink;
import dev.skycraft.world.SkyCollision;
import dev.skycraft.world.SkyTri;
import dev.skycraft.world.TriCollider;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.item.ItemEntity;
import net.minecraft.world.phys.Vec3;
import java.util.ArrayList;
import java.util.List;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;
@Mixin(Entity.class)
public abstract class ItemGroundMixin {
    @Inject(method="collide",at=@At("RETURN"),cancellable=true)
    private void classicItemGround(Vec3 movement,CallbackInfoReturnable<Vec3> result){
        if(!((Object)this instanceof ItemEntity item) || !SkyLink.active() || item.noPhysics)return;
        var box=item.getBoundingBox();var move=result.getReturnValue();List<SkyTri> triangles=new ArrayList<>();
        SkyCollision.trianglesNear(box.expandTowards(move).inflate(item.getBbWidth()),triangles);
        if(triangles.isEmpty())return;
        double[] r=TriCollider.resolve(triangles,(box.minX+box.maxX)*.5,box.minY,(box.minZ+box.maxZ)*.5,box.getXsize()*.5,box.getYsize(),0,item.onGround(),move.x,move.y,move.z);
        result.setReturnValue(Entity.collideBoundingBox(item,new Vec3(r[0],r[1],r[2]),box,item.level(),List.of()));
    }
}
