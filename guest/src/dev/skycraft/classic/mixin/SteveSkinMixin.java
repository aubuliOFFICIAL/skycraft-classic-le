package dev.skycraft.classic.mixin;
import dev.skycraft.classic.ClassicRules;
import net.minecraft.client.Minecraft;
import net.minecraft.client.player.AbstractClientPlayer;
import net.minecraft.core.ClientAsset;
import net.minecraft.resources.Identifier;
import net.minecraft.world.entity.player.PlayerSkin;
import net.minecraft.world.entity.player.PlayerModelType;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;
@Mixin(AbstractClientPlayer.class)
public abstract class SteveSkinMixin {
    private static final PlayerSkin CLASSIC_STEVE=PlayerSkin.insecure(new ClientAsset.ResourceTexture(Identifier.withDefaultNamespace("entity/player/wide/steve"),Identifier.parse(ClassicRules.Steve.texture)),null,null,PlayerModelType.WIDE);
    @Inject(method="getSkin",at=@At("HEAD"),cancellable=true)
    private void classicSteve(CallbackInfoReturnable<PlayerSkin> result){
        if((Object)this==Minecraft.getInstance().player)result.setReturnValue(CLASSIC_STEVE);
    }
}
